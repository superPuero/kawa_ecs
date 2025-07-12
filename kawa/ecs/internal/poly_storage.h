#ifndef KW_ECS_POLY_STORAGE
#define	KW_ECS_POLY_STORAGE

#include <memory>
#include <new>
#include <cstring> 

#include <iostream>
#include "core.h"

namespace kawa
{
namespace ecs 
{
	namespace _internal
	{
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
			inline void populate(size_t capacity) noexcept
			{
				if (!_populated)
				{
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

					_mask = new bool[capacity]();
					_connector = new size_t[capacity]();
					_r_connector = new size_t[capacity]();

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
								KW_ASSERT_MSG(false, std::string("trying to copy uncopyable type") + typeid(T).name());
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
								KW_ASSERT_MSG(false, std::string("trying to move unmovable type") + typeid(T).name());
							}
						};


					_populated = true;
				}
			}
		public:
			inline bool has(size_t index) noexcept
			{
				KW_ASSERT(_validate_index(index));

				return _mask[index];
			}

			template<typename T, typename...Args>
			inline T& emplace(size_t index, Args&&...args)  noexcept
			{
				KW_ASSERT(_validate_index(index));

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
			inline T& get(size_t index) noexcept
			{
				KW_ASSERT(_validate_index(index));

				return *(static_cast<T*>(_storage) + index);
			}

			template<typename T>
			inline T* get_if_has(size_t index) noexcept
			{
				KW_ASSERT(_validate_index(index));

				if (_mask[index])
				{
					return static_cast<T*>(_storage) + index;
				}
				return nullptr;
			}

			inline void erase(size_t index) noexcept
			{
				KW_ASSERT(_validate_index(index));

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
				KW_ASSERT(_validate_index(from));
				KW_ASSERT(_validate_index(to));

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
				KW_ASSERT(_validate_index(from));
				KW_ASSERT(_validate_index(to));

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

			inline bool _validate_index(size_t id) noexcept
			{
				if (id >= _capacity)
				{
					std::cout << "out of bounds storage access" << '\n';
					return false;
				}

				return true;
			}

			void* _storage = nullptr;
			bool* _mask = nullptr;
			size_t* _connector = nullptr;
			size_t* _r_connector = nullptr;
			size_t			_occupied = 0;

			erase_fn_t		_erase_fn = nullptr;
			delete_fn_t		_delete_fn = nullptr;
			copy_fn_t		_copy_fn = nullptr;
			move_fn_t		_move_fn = nullptr;
			size_t			_capacity = 0;

			bool			_populated = false;

		};

	}
}
}
#endif