#ifndef KAWA_INDIRECT_ARRAY
#define KAWA_INDIRECT_ARRAY

#include "core_types.h"
#include "any.h"

namespace kawa
{
	struct indirect_array_base
	{
		bool contains(usize index) const noexcept
		{
			return _mask[index];
		}

		usize* begin() const noexcept
		{
			return _indirect_map;
		}

		usize* end() const noexcept
		{
			return _indirect_map + _occupied;
		}

		u8* _storage = nullptr;
		usize _capacity = 0;

		bool* _mask = nullptr;
		usize* _indirect_map = nullptr;
		usize* _reverse_indirect_map = nullptr;
		usize _occupied = 0;
	};



	template<typename T>
	struct indirect_array : indirect_array_base
	{
		struct iterator
		{
			iterator(indirect_array& s) noexcept : self(s) {}
			iterator(indirect_array& s, usize i) noexcept : self(s), current(i) {}

			iterator& operator++() noexcept
			{
				current++;
				return *this;
			}

			T& operator*() noexcept
			{
				return *(reinterpret_cast<T*>(self._storage) + self._indirect_map[current]);
			}

			bool operator==(const iterator& other) const noexcept
			{
				return current == other.current;
			}

			indirect_array& self;
			usize current = 0;
		};

		indirect_array(usize capacity) noexcept
		{
			refresh(capacity);
		};

		indirect_array& operator=(const indirect_array& other) noexcept
		{
			if (this != &other)
			{
				release();			

				_capacity = other._capacity;
				_occupied = other._occupied;

				_storage = (u8*)::operator new(_capacity * sizeof(T), std::align_val_t{ alignof(T) });
				_mask = new bool[_capacity] {};
				_indirect_map = new usize[_capacity]{};
				_reverse_indirect_map = new usize[_capacity]{};

				memcpy(_mask, other._mask, _capacity * sizeof(bool));
				memcpy(_indirect_map, other._indirect_map, _capacity * sizeof(usize));
				memcpy(_reverse_indirect_map, other._reverse_indirect_map, _capacity * sizeof(usize));

				for (usize i = 0; i < other._occupied; i++)
				{
					usize idx = _indirect_map[i];
					new (_storage + (sizeof(T) * idx)) T(other.get(idx));
				}
			}

			return *this;
		}

		indirect_array(const indirect_array& other) noexcept
		{
			*this = other;
		}

		indirect_array& operator=(indirect_array&& other) noexcept
		{
			if (this != &other)
			{
				release();

				_storage = other._storage;
				_capacity = other._capacity;
				_mask = other._mask;
				_indirect_map = other._indirect_map;
				_reverse_indirect_map = other._reverse_indirect_map;
				_occupied = other._occupied;

				other._storage = nullptr;
				other._occupied = 0;

				//other.refresh(_capacity);
			}

			return *this;
		}


		indirect_array(indirect_array&& other) noexcept
		{
			*this = std::move(other);
		}


		~indirect_array() noexcept
		{
			release();
		}

		void release() noexcept
		{
			if (_storage)
			{
				for (usize i = 0; i < _occupied; i++)
				{
					reinterpret_cast<T*>(_storage)[_indirect_map[i]].~T();
				}

				::operator delete(_storage, _capacity * sizeof(T), std::align_val_t{ alignof(T) });

				_storage = nullptr;

				delete[] _mask;
				delete[] _indirect_map;
				delete[] _reverse_indirect_map;
				_occupied = 0;
			}

		}

		void refresh(usize capacity)
		{
			release();

			_storage = (u8*)::operator new(capacity * sizeof(T), std::align_val_t{ alignof(T) });
			_mask = new bool[capacity]{};
			_indirect_map = new usize[capacity]{};
			_reverse_indirect_map = new usize[capacity]{};

			_capacity = capacity;
		}

		template<typename...Args>
		T& emplace(usize index, Args&&...args) noexcept
		{
			kw_assert(index < _capacity);

			T* out = reinterpret_cast<T*>(_storage) + index;

			bool& cell = _mask[index];

			if (!cell)
			{
				cell = true;

				usize _indirect_index = _occupied++;

				_reverse_indirect_map[index] = _indirect_index;
				_indirect_map[_indirect_index] = index;
			}
			else
			{
				out->~T();
			}

			new (out) T(std::forward<Args>(args)...);

			return *out;
		}

		void erase(usize index) noexcept
		{
			kw_assert(index < _capacity);

			bool& cell = _mask[index];

			if (cell)
			{
				reinterpret_cast<T*>(_storage)[index].~T();

				usize _indirect_index = _reverse_indirect_map[index];

				_occupied--;
				_indirect_map[_indirect_index] = _indirect_map[_occupied];
				_reverse_indirect_map[_indirect_map[_occupied]] = _indirect_index;

				cell = false;
			}
		}

		T& get(usize index) noexcept
		{
			kw_assert(index < _capacity);
			kw_assert(_mask[index]);

			return reinterpret_cast<T*>(_storage)[index];
		}

		const T& get(usize index) const noexcept
		{
			kw_assert(index < _capacity);
			kw_assert(_mask[index]);

			return reinterpret_cast<T*>(_storage)[index];
		}

		T* try_get(usize index) noexcept
		{
			bool& cell = _mask[index];

			if (cell && (index < _capacity))
			{
				return reinterpret_cast<T*>(_storage) + index;
			}

			return nullptr;
		}

		const T* try_get(usize index) const noexcept
		{
			bool& cell = _mask[index];

			if (cell && (index < _capacity))
			{
				return reinterpret_cast<T*>(_storage) + index;
			}

			return nullptr;
		}

		usize occupied() noexcept
		{
			return _occupied;
		}

		indirect_array_base& as_base() noexcept
		{
			return *(indirect_array_base*)this;
		}

		bool empty() noexcept
		{
			return !_occupied;
		}

		T& operator[](usize index) noexcept
		{
			return get(index);
		}

		const T& operator[](usize index) const noexcept
		{
			return get(index);
		}

		iterator begin() noexcept
		{
			return { *this };
		}

		iterator end() noexcept
		{
			return { *this, _occupied };
		}
	};

}

#endif // !KAWA_INDIRECT_ARRAY
