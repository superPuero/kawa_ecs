#ifndef KAWA_THREAD_POOL
#define	KAWA_THREAD_POOL

#include <thread>
#include <barrier>
#include <new>
#include <functional>

namespace kawa
{
class thread_pool
{
public:
	thread_pool(size_t thread_count)
		: _barrier(thread_count + 1)
		, _thread_count(thread_count)
		, _tasks_count(thread_count + 1)
	{
		_starts = new size_t[_tasks_count]();
		_ends = new size_t[_tasks_count]();

		if (_thread_count)
		{
			_threads = (std::thread*)::operator new(sizeof(std::thread) * _thread_count, std::align_val_t{ alignof(std::thread) });

			for (size_t i = 0; i < _thread_count; i++)
			{
				new (_threads + i) std::thread
				(
					[this, i]()
					{
						while (true)
						{
							_barrier.arrive_and_wait();

							if (_should_join)
							{
								_barrier.arrive_and_wait();
								return;
							}

							_task(_starts[i], _ends[i]);

							_barrier.arrive_and_wait();
						}
					}
				);
			}
		}
	}
	~thread_pool()
	{
		_should_join = true;

		_barrier.arrive_and_wait();

		_barrier.arrive_and_wait();

		for (size_t i = 0; i < _thread_count; i++)
		{
			_threads[i].join();
		}

		if (_thread_count)
		{
			::operator delete(_threads, std::align_val_t{ alignof(std::thread) });
		}

		delete[] _ends;
		delete[] _starts;
	}



public:
	template<typename Fn>
	void task(Fn&& query, size_t work)
	{
		_task = std::forward<Fn>(query);

		size_t chunk_work = work / _tasks_count;

		size_t tail_chunk_work = work - chunk_work * _tasks_count;

		for (size_t i = 0; i < _tasks_count; ++i)
		{
			size_t start = i * chunk_work;
			size_t end = (i == (_tasks_count - 1)) ? start + chunk_work + tail_chunk_work : start + chunk_work;

			if (start >= end) continue;

			_starts[i] = start;
			_ends[i] = end;
		}

		_barrier.arrive_and_wait();

		_task(_starts[_tasks_count - 1], _ends[_tasks_count - 1]);

		_barrier.arrive_and_wait();
	}


private:
	std::thread*							_threads = nullptr;
	const size_t							_thread_count = 0;
	const size_t							_tasks_count = 0;

	size_t*									_starts = nullptr;
	size_t*									_ends = nullptr;


	std::function<void(size_t, size_t)>		_task = nullptr;
	std::barrier<>							_barrier;

	bool									_should_join = false;
};

}
#endif