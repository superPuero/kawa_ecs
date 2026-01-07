#ifndef KAWA_TASK_MANAGER
#define KAWA_TASK_MANAGER

#include <semaphore>
#include "core_types.h"

namespace kawa
{
	using task_fn = std::function<void()>;

	struct alignas(64) worker_entry
	{
		task_fn task = nullptr;
		thread thread;
		std::binary_semaphore semaphore{ 0 };

		atomic<u32> generation{ 0 };
	};

	struct task_handle 
	{
		constexpr static u32 _invalid_worker = std::numeric_limits<u32>::max();
		u32 worker = 0;
		u32 generation = 0;
	};

	enum class task_schedule_policy
	{
		ensure,
		wait_if_neccesary,
		try_schedule
	};

	class task_manager
	{
	public:
		task_manager(usize worker_count) noexcept
		{
			_workers.reserve(worker_count);

			for (usize i = 0; i < worker_count; i++)
			{
				auto& w = _workers.emplace_back(new worker_entry());

				w->thread = std::move(thread(worker_loop, std::ref(*this), std::ref(*w)));
			}
		}

		~task_manager() noexcept
		{
			exit.store(true, std::memory_order_release);

			for (auto w : _workers) w->semaphore.release();
			for (auto w : _workers) w->thread.join();
			for (auto w : _workers) delete w;
		}

		void static worker_loop(task_manager& manager, worker_entry& worker) noexcept
		{
			while (true)
			{
				worker.semaphore.acquire();

				if (manager.exit.load(std::memory_order_acquire)) return;

				if (worker.task)
				{
					worker.task();
				}

				worker.generation.fetch_add(1, std::memory_order_release);
			}
		}

		task_handle schedule(task_fn&& task, task_schedule_policy policy = task_schedule_policy::wait_if_neccesary) noexcept
		{
			switch (policy)
			{
			case kawa::task_schedule_policy::ensure:	
				return _schedule_ensure(std::forward<task_fn>(task));
				break;
			case kawa::task_schedule_policy::wait_if_neccesary:
				return _schedule_wait_if_necessary(std::forward<task_fn>(task));
				break;
			case kawa::task_schedule_policy::try_schedule:
				return _schedule_try(std::forward<task_fn>(task));
				break;
			default:
				kw_panic();
				break;
			}
		}

		task_handle _schedule_try(task_fn&& task) noexcept
		{
			for (u32 i = 0; i < _workers.size(); i++)
			{
				auto& w = *_workers[i];

				u32 gen = w.generation.load(std::memory_order_relaxed);
				if (gen % 2 != 0) continue;

				if (w.generation.compare_exchange_strong(gen, gen + 1, std::memory_order_acquire))
				{
					w.task = std::forward<task_fn>(task);

					w.semaphore.release();

					return task_handle{ .worker = i, .generation = (gen + 1)};
				}
			}

			return task_handle{ .worker = task_handle::_invalid_worker };
		}

		task_handle _schedule_ensure(task_fn&& task) noexcept
		{
			auto th = _schedule_try(std::forward<task_fn>(task));
			kw_assert(th.worker != task_handle::_invalid_worker);
			return th;
		}

		task_handle _schedule_wait_if_necessary(task_fn&& task)
		{
			task_handle out;

			while (true)
			{
				out = _schedule_try(std::forward<task_fn>(task));
				if (out.worker != task_handle::_invalid_worker) return out;
				std::this_thread::yield();
			}
		}

		void wait(const task_handle& th) noexcept
		{
			if (th.worker == task_handle::_invalid_worker)
				return;

			auto& w = *_workers[th.worker];

			while (w.generation.load(std::memory_order_acquire) == th.generation)
			{
				std::this_thread::yield();
			}
		}

		void wait(const dyn_array<task_handle>& tasks) noexcept
		{
			for (auto& t : tasks)
				wait(t);
		}

		dyn_array<worker_entry*> _workers;
		atomic<bool> exit;
	};
}


#endif KAWA_TASK_MANAGER
