#ifndef KAWA_RING_BUFFER
#define KAWA_RING_BUFFER

#include "core_types.h"

namespace kawa
{
	template<typename T, usize size>
	class ring_buffer
	{
	public:
		template<typename...Args>
		T& put(Args&&...args) noexcept
		{
			kw_assert_msg((_occupied + 1) <= size, "ring_buffer overflow, max size is {}", size);

			auto v = std::construct_at<T>(reinterpret_cast<T*>(_container) + ((_curr + _occupied) % size), std::forward<Args>(args)...);		
			_occupied++;

			return *v;
		}

		T& current() noexcept
		{
			kw_assert(_occupied);
			return *(reinterpret_cast<T*>(_container) + _curr);
		}

		void pop() noexcept
		{
			std::destroy_at(reinterpret_cast<T*>(_container) + _curr);
			_curr = (_curr + 1) % size;
			_occupied--;
		}

		bool empty() noexcept
		{
			return !_occupied;
		}

		usize occupied() noexcept
		{
			return _occupied;
		}

	private:
		u8 _container[sizeof(T) * size]{};
		usize _occupied = 0;
		usize _curr = 0;
	};
}

#endif KAWA_RING_BUFFER