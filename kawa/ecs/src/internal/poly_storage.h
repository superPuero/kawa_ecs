#ifndef KAWA_POLY_STORAGE
#define	KAWA_POLY_STORAGE

#include <memory>
#include <new>
#include <cstring> 

#include <iostream>

namespace kawa
{

	struct empty_poly_storage_type_t {};

	class poly_storage
	{
		using delete_fn_t = void(*)(void*);
		using erase_fn_t = void(*)(void*, size_t);
		using copy_fn_t = void(*)(void*, size_t, size_t);
		using move_fn_t = void(*)(void*, size_t, size_t);

	public:
		inline poly_storage() noexcept = default;
		inline ~poly_storage() noexcept
		{
			if (_populated)
			{
				for (size_t i = 0; i < _occupied; i++)
				{
					size_t id = _connector[i];
					_erase_fn(_storage, id);
				}

				_delete_fn(_storage);

				delete[] _mask;
				delete[] _connector;
				delete[] _r_connector;

				_populated = false;
			}
		}

	public:
		template<typename T>
		inline poly_storage* populate(size_t capacity) noexcept
		{
			KAWA_ASSERT_MSG(!_populated, "poly_storage<{}>::populate<{}> on already populated storage", _type_info.name, meta::type_name<T>());

			_type_info = meta::type_info::create<T>();

			_capacity = capacity;

			if constexpr (!std::is_empty_v<T>)
			{
				_storage = ::operator new(sizeof(T) * capacity, std::align_val_t{ alignof(T) });
			}

			_delete_fn =
				[](void* data)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					::operator delete(data, std::align_val_t{ alignof(T) });
				};

			_mask			= new bool[capacity]();
			_connector		= new size_t[capacity]();
			_r_connector	= new size_t[capacity]();

			_erase_fn =
				[](void* data, size_t index)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					(static_cast<T*>(data) + index)->~T();
				};

			_copy_fn = [](void* data, size_t from, size_t to)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					if constexpr (std::is_copy_constructible_v<T>)
					{
						new (static_cast<T*>(data) + to) T(*(static_cast<T*>(data) + from));
					}
					else
					{
						KAWA_VERIFY_MSG(false, "trying to copy uncopyable type [ {} ]", meta::type_name<T>());
					}
				};

			_move_fn = [](void* data, size_t from, size_t to)
				{
					if constexpr (std::is_empty_v<T>)
					{
						return;
					}

					if constexpr (std::is_move_constructible_v<T>)
					{
						new (static_cast<T*>(data) + to) T(std::move(*(static_cast<T*>(data) + from)));
					}
					else
					{
						KAWA_VERIFY_MSG(false, "trying to move unmovable type [ {} ]", meta::type_name<T>());
					}
				};


			_populated = true;

			return this;
		}

	public:
		inline const meta::type_info& get_type_info() const noexcept
		{
			return _type_info;
		}

		inline bool has(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::has out of bounds index [ {} ] access", _type_info.name, index);

			return _mask[index];
		}

		template<typename T, typename...Args>
		inline T& emplace(size_t index, Args&&...args) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::emplace on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::emplace out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::emplace invalid type [ {} ] access", _type_info.name, meta::type_name<T>());

			bool& cell = _mask[index];
			if (!cell)
			{
				size_t l_idx = _occupied++;

				_connector[l_idx] = index;
				_r_connector[index] = l_idx;

				cell = true;

				return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
			}
			else
			{
				_erase_fn(_storage, index);

				return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
			}

		}

		template<typename T>
		inline T& get(size_t index) const  noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::get invalid type [ {} ] access", _type_info.name, meta::type_name<T>());
			KAWA_ASSERT_MSG(_mask[index], "poly_storage<{}>::get of uninitialized [ {} ]", _type_info.name, index);

			return *(static_cast<T*>(_storage) + index);
		}

		template<typename T>
		inline T* get_if_has(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get_if_has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get_if_has out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<T>(), "poly_storage<{}>::get_if_has invalid type [ {} ] access", _type_info.name, meta::type_name<T>());

			if (!_mask[index])
			{
				return nullptr;
			}

			return static_cast<T*>(_storage) + index;
		}

		template<typename T>
		inline std::enable_if_t<std::is_pointer_v<T>, T> get_defer(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get_if_has on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get_if_has out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<std::remove_pointer_t<T>>(), "poly_storage<{}>::get_if_has invalid type [ {} ] access", _type_info.name, meta::type_name< std::remove_pointer_t<T>>());

			if (!_mask[index])
			{
				return nullptr;
			}

			return static_cast<T>(_storage) + index;
		}

		template<typename T>
		inline std::enable_if_t<!std::is_pointer_v<T>, T&> get_defer(size_t index) const noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::get on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::get out of bounds index [ {} ] access", _type_info.name, index);
			KAWA_ASSERT_MSG(_validate_type<std::remove_reference_t<T>>(), "poly_storage<{}>::get invalid type [ {} ] access", _type_info.name, meta::type_name<std::remove_reference_t<T>>());
			KAWA_ASSERT_MSG(_mask[index], "poly_storage<{}>::get of uninitialized [ {} ]", _type_info.name, index);

			return *(static_cast<std::remove_reference_t<T>*>(_storage) + index);
		}

		inline void erase(size_t index) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::erase on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(index), "poly_storage<{}>::erase out of bounds index [ {} ] access", _type_info.name, index);

			bool& cell = _mask[index];
			if (cell)
			{
				size_t l_idx = _r_connector[index];

				_erase_fn(_storage, index);

				_connector[l_idx] = _connector[--_occupied];
				_r_connector[_connector[_occupied]] = l_idx;

				cell = false;
			}
		}

		inline void copy(size_t from, size_t to) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::copy on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::copy out of bounds index [ {} ] access", _type_info.name, from);
			KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::copy out of bounds index [ {} ] access", _type_info.name, to);

			if (has(from))
			{
				bool& cell = _mask[to];
				if (!cell)
				{
					size_t l_idx = _occupied++;

					_connector[l_idx] = to;
					_r_connector[to] = l_idx;

					cell = true;

					_copy_fn(_storage, from, to);
				}
				else
				{
					_erase_fn(_storage, to);
					_copy_fn(_storage, from, to);
				}
			}
		}

		inline void move(size_t from, size_t to) noexcept
		{
			KAWA_ASSERT_MSG(_populated, "poly_storage<->::move on non populated storage");
			KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, from);
			KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, to);

			if (has(from))
			{
				bool& cell = _mask[to];
				if (!cell)
				{
					size_t l_idx = _occupied++;

					_connector[l_idx] = to;
					_r_connector[to] = l_idx;

					cell = true;

					_move_fn(_storage, from, to);
				}
				else
				{
					_erase_fn(_storage, to);
					_move_fn(_storage, from, to);
				}

				erase(from);
			}
		}

		inline bool is_populated() const noexcept
		{
			return _populated;
		}

		inline size_t* begin() noexcept
		{
			return _connector;
		}

		inline size_t* end() noexcept
		{
			return _connector + _occupied;
		}

		inline size_t get_at(size_t i) noexcept
		{
			return _connector[i];
		}

		inline size_t occupied() noexcept
		{
			return _occupied;
		}

		inline size_t capcatiy() noexcept
		{
			return _capacity;
		}

	private:
		inline bool _validate_index(size_t id) const noexcept
		{
			if (id >= _capacity)
			{
				return false;
			}

			return true;
		}

		template<typename T>
		inline bool _validate_type() const noexcept
		{
			if (_type_info.hash != meta::type_hash<T>())
			{
				return false;
			}
			return true;
		}

		void*			_storage = nullptr;
		bool*			_mask = nullptr;
		size_t*			_connector = nullptr;
		size_t*			_r_connector = nullptr;
		size_t			_occupied = 0;

		erase_fn_t		_erase_fn = nullptr;
		delete_fn_t		_delete_fn = nullptr;
		copy_fn_t		_copy_fn = nullptr;
		move_fn_t		_move_fn = nullptr;
		size_t			_capacity = 0;

		bool			_populated = false;

		meta::type_info	_type_info;
	};
}
#endif