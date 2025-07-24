#ifndef KAWA_ECS_POLY_STORAGE
#define	KAWA_ECS_POLY_STORAGE

#include <memory>
#include <new>
#include <cstring> 

#include <iostream>
#include "ecs_base.h"

namespace kawa
{
namespace ecs
{
namespace _
{
class poly_storage
{
	using delete_fn_t = void(*)(void*);
	using erase_fn_t = void(*)(void*, size_t);
	using copy_fn_t = void(*)(void*, void*, size_t, size_t);
	using move_fn_t = void(*)(void*, void*, size_t, size_t);

	using handler_invoke_fn_t = void(*)(void*, void*, entity_id);
	using copy_handler_fn_t = void(*)(void*, void*&);

public:
	inline poly_storage() noexcept = default;

	inline poly_storage(const poly_storage& other) noexcept
		: _type_info(other._type_info)
		, _capacity(other._capacity)
		, _occupied(other._occupied)
	{
		if (other._populated)
		{
			if (other._storage)
			{			
				_storage = ::operator new(_type_info.size * _capacity, _type_info.alignment);
			}

			_delete_fn = other._delete_fn;
			_erase_fn = other._erase_fn;
			_copy_fn = other._copy_fn;
			_move_fn = other._move_fn;

			if (other._on_construct_fn)
			{
				_on_construct_fn_copy_fn = other._on_construct_fn_copy_fn;
				_on_construct_fn_copy_fn(other._on_construct_fn, _on_construct_fn);
				_on_construct_invoke_fn = other._on_construct_invoke_fn;
				_on_construct_delete_fn = other._on_construct_delete_fn;
			}

			if (other._on_destroy_fn)
			{
				_on_destroy_fn_copy_fn = other._on_destroy_fn_copy_fn;
				_on_destroy_fn_copy_fn(other._on_destroy_fn, _on_destroy_fn);
				_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
				_on_destroy_delete_fn = other._on_destroy_delete_fn;
			}

			_mask = new bool[_capacity];
			std::copy(other._mask, other._mask + _capacity, _mask);

			_connector = new size_t[_capacity];
			std::copy(other._connector, other._connector + _capacity, _connector);

			_r_connector = new size_t[_capacity];
			std::copy(other._r_connector, other._r_connector + _capacity, _r_connector);

			for (size_t i = 0; i < _occupied; i++)
			{
				size_t id = _connector[i];
				if (_mask[id])
				{
					_copy_fn(other._storage, _storage, id, id);
					_on_construct(id);
				}
			}

			_populated = true;
		}
	}

	inline poly_storage(poly_storage&& other) noexcept
		: _type_info(other._type_info)
		, _capacity(other._capacity)
		, _occupied(other._occupied)
		, _storage(other._storage)
		, _mask(other._mask)
		, _connector(other._connector)
		, _r_connector(other._r_connector)
		, _delete_fn(other._delete_fn)
		, _erase_fn(other._erase_fn)
		, _copy_fn(other._copy_fn)
		, _move_fn(other._move_fn)
		, _on_construct_fn(other._on_construct_fn)
		, _on_destroy_fn(other._on_destroy_fn)
		, _on_construct_invoke_fn(other._on_construct_invoke_fn)
		, _on_destroy_invoke_fn(other._on_destroy_invoke_fn)
		, _on_construct_delete_fn(other._on_construct_delete_fn)
		, _on_destroy_delete_fn(other._on_destroy_delete_fn)
		, _populated(other._populated)
	{
		KAWA_ASSERT_MSG(other._populated, "poly_storage::poly_storage(poly_storage&& other) on unpopulated storage");

		other._populated = false;
	}

	inline poly_storage& operator=(const poly_storage& other) noexcept
	{
		if (this != &other)
		{
			this->clear();

			if (other._populated)
			{
				_type_info = other._type_info;
				_capacity = other._capacity;
				_occupied = other._occupied;

				if (other._storage)
				{
					_storage = ::operator new(_type_info.size * _capacity, _type_info.alignment);
				}

				_delete_fn = other._delete_fn;
				_erase_fn = other._erase_fn;
				_copy_fn = other._copy_fn;
				_move_fn = other._move_fn;

				if (other._on_construct_fn)
				{
					_on_construct_fn_copy_fn = other._on_construct_fn_copy_fn;
					_on_construct_fn_copy_fn(other._on_construct_fn, _on_construct_fn);
					_on_construct_invoke_fn = other._on_construct_invoke_fn;
					_on_construct_delete_fn = other._on_construct_delete_fn;
				}

				if (other._on_destroy_fn)
				{
					_on_destroy_fn_copy_fn = other._on_destroy_fn_copy_fn;
					_on_destroy_fn_copy_fn(other._on_destroy_fn, _on_destroy_fn);
					_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
					_on_destroy_delete_fn = other._on_destroy_delete_fn;
				}

				_mask = new bool[_capacity];
				std::copy(other._mask, other._mask + _capacity, _mask);

				_connector = new size_t[_capacity];
				std::copy(other._connector, other._connector + _capacity, _connector);

				_r_connector = new size_t[_capacity];
				std::copy(other._r_connector, other._r_connector + _capacity, _r_connector);

				for (size_t i = 0; i < _occupied; i++)
				{
					size_t id = _connector[i];
					if (_mask[id])
					{
						_copy_fn(other._storage, _storage, id, id);
						_on_construct(id);
					}
				}

				this->_populated = true;
			}
		}

		return *this;
	}

	inline poly_storage& operator=(poly_storage&& other) noexcept
	{
		if (this != &other)
		{
			this->clear();

			if (other._populated)
			{
				_type_info = other._type_info;
				_capacity = other._capacity;
				_occupied = other._occupied;
				_storage = other._storage;
				_mask = other._mask;
				_connector = other._connector;
				_r_connector = other._r_connector;
				_delete_fn = other._delete_fn;
				_erase_fn = other._erase_fn;
				_copy_fn = other._copy_fn;
				_move_fn = other._move_fn;
				_on_construct_fn = other._on_construct_fn;
				_on_destroy_fn = other._on_destroy_fn;
				_on_construct_invoke_fn = other._on_construct_invoke_fn;
				_on_destroy_invoke_fn = other._on_destroy_invoke_fn;
				_on_construct_delete_fn = other._on_construct_delete_fn;
				_on_destroy_delete_fn = other._on_destroy_delete_fn;

				other._populated = false;
								   
				this->_populated = true;
			}
		}

		return *this;
	}

	inline ~poly_storage() noexcept
	{
		clear();
	}


public:
	template<typename T>
	inline poly_storage* populate(size_t capacity) noexcept
	{
		KAWA_ASSERT_MSG(!_populated, "poly_storage<{}>::populate<{}> on already populated storage", _type_info.name, meta::type_name<T>());

		_type_info = meta::type_info::create<T>();

		_capacity = capacity;

		if  constexpr (!std::is_empty_v<T>)
		{
			_storage = ::operator new(sizeof(T) * capacity, _type_info.alignment);
		}
		else
		{
			if constexpr (!std::is_trivially_destructible_v<T>)
			{
				_storage = ::operator new(sizeof(T) * 1, _type_info.alignment);
			}
		}

		_delete_fn =
			[](void* data)
			{
				::operator delete(data, std::align_val_t{ alignof(T) });
			};

		_mask = new bool[capacity]();
		_connector = new size_t[capacity]();
		_r_connector = new size_t[capacity]();

		_erase_fn =
			[](void* data, size_t index)
			{
				if constexpr (!std::is_empty_v<T>)
				{
					(static_cast<T*>(data) + index)->~T();
				}
				else
				{
					if constexpr (!std::is_trivially_destructible_v<T>)
					{
						(static_cast<T*>(data))->~T();
					}
				}
			};

		_copy_fn = [](void* from_ptr, void* to_ptr, size_t from, size_t to)
			{
				if constexpr (std::is_copy_constructible_v<T>)
				{
					new (static_cast<T*>(to_ptr) + to) T(*(static_cast<T*>(from_ptr) + from));
				}
				else
				{
					KAWA_VERIFY_MSG(false, "trying to copy uncopyable type [ {} ]", meta::type_name<T>());
				}
			};

		_move_fn = [](void* from_ptr, void* to_ptr, size_t from, size_t to)
			{
				if constexpr (std::is_move_constructible_v<T>)
				{
					new (static_cast<T*>(to_ptr) + to) T(std::move(*(static_cast<T*>(from_ptr) + from)));
				}
				else
				{
					KAWA_VERIFY_MSG(false, "trying to move unmovable type [ {} ]", meta::type_name<T>());
				}
			};


		_populated = true;

		return this;
	}

	inline void clear() noexcept
	{
		if (_populated)
		{
			for (size_t i = 0; i < _occupied; i++)
			{
				size_t id = _connector[i];

				_on_destory(id);
				_erase_fn(_storage, id);
			}

			_delete_fn(_storage);

			if (_on_construct_fn)
			{
				_on_construct_delete_fn(_on_construct_fn);
				_on_construct_fn = nullptr;
			}

			if (_on_destroy_fn)
			{
				_on_destroy_delete_fn(_on_destroy_fn);
				_on_destroy_fn = nullptr;
			}

			delete[] _mask;
			delete[] _connector;
			delete[] _r_connector;

			_populated = false;
		}
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

			T& val = *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));

			_on_construct(index);

			return val;
		}
		else
		{
			_on_destory(index);

			_erase_fn(_storage, index);

			return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
		}
	}

	template<typename T>
	inline T& get(size_t index) const noexcept
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
		//requires (!std::is_pointer_v<T>)
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

			_on_destory(index);
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

		if (_mask[from])
		{
			bool& cell = _mask[to];
			if (!cell)
			{
				size_t l_idx = _occupied++;

				_connector[l_idx] = to;
				_r_connector[to] = l_idx;

				cell = true;

				_copy_fn(_storage, _storage, from, to);
				_on_construct(to);
			}
			else
			{
				_on_destory(to);
				_erase_fn(_storage, to);
				_copy_fn(_storage, _storage, from, to);
				_on_construct(to);

			}
		}
	}

	inline void move(size_t from, size_t to) noexcept
	{
		KAWA_ASSERT_MSG(_populated, "poly_storage<->::move on non populated storage");
		KAWA_ASSERT_MSG(_validate_index(from), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, from);
		KAWA_ASSERT_MSG(_validate_index(to), "poly_storage<{}>::move out of bounds index [ {} ] access", _type_info.name, to);

		if (_mask[from])
		{
			bool& cell = _mask[to];
			if (!cell)
			{
				size_t l_idx = _occupied++;

				_connector[l_idx] = to;
				_r_connector[to] = l_idx;

				cell = true;

				_move_fn(_storage, _storage, from, to);
				_on_construct(to);
			}
			else
			{
				_on_destory(to);
				_erase_fn(_storage, to);
				_move_fn(_storage, _storage, from, to);
				_on_construct(to);
			}

			_on_destory(from);
			erase(from);
		}
	}

	template<typename Fn>
	inline void set_on_construct(Fn&& fn) noexcept
	{
		if (_on_construct_fn)
		{
			_on_construct_delete_fn(_on_construct_fn);
		}

		_on_construct_fn = new Fn(std::forward<Fn>(fn));

		_on_construct_delete_fn =
			[](void* fn)
			{
				delete static_cast<Fn*>(fn);
			};

		_on_construct_invoke_fn =
			[](void* fn, void* data, entity_id entity)
			{
				using T = typename meta::template function_traits<Fn>::template arg_at<1>;
				static_cast<Fn*>(fn)->operator()(entity, *(static_cast<std::remove_reference_t<T>*>(data) + entity));
			};

		_on_construct_fn_copy_fn = 
			[](void* from, void*& to)
			{
				to = new Fn(*static_cast<Fn*>(from));
			};
	}

	template<typename Fn>
	inline void set_on_destroy(Fn&& fn) noexcept
	{
		if (_on_destroy_fn)
		{
			_on_destroy_delete_fn(_on_construct_fn);
		}

		_on_destroy_fn = new Fn(std::forward<Fn>(fn));

		_on_destroy_delete_fn =
			[](void* fn)
			{
				delete static_cast<Fn*>(fn);
			};

		_on_destroy_invoke_fn =
			[](void* fn, void* data, entity_id entity)
			{
				using T = typename meta::template function_traits<Fn>::template arg_at<1>;
				static_cast<Fn*>(fn)->operator()(entity, *(static_cast<std::remove_reference_t<T>*>(data) + entity));
			};

		_on_destroy_fn_copy_fn =
			[](void* from, void*& to)
			{
				to = new Fn(*static_cast<Fn*>(from));
			};
	}

	inline void remove_on_construct() noexcept
	{
		if (_on_construct_fn)
		{
			_on_construct_delete_fn(_on_construct_fn);
			_on_construct_fn = nullptr;
		}
	}

	inline void remove_on_destroy() noexcept
	{
		if (_on_destroy_fn)
		{
			_on_destroy_delete_fn(_on_construct_fn);
			_on_destroy_fn = nullptr;
		}
	}

	inline bool has_on_construct() const noexcept
	{
		return _on_construct_fn;
	}

	inline bool has_on_destroy() const noexcept
	{
		return _on_destroy_fn;
	}

	inline bool is_populated() const noexcept
	{
		return _populated;
	}

	inline entity_id* begin() noexcept
	{
		return _connector;
	}

	inline entity_id* end() noexcept
	{
		return _connector + _occupied;
	}

	inline entity_id get(size_t i) const noexcept
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

	inline entity_id operator[](size_t i) const noexcept
	{
		return get(i);
	}

private:
	inline void _on_destory(entity_id id) noexcept
	{
		if (_on_destroy_fn)
		{
			_on_destroy_invoke_fn(_on_destroy_fn, _storage, id);
		}
	}

	inline void _on_construct(entity_id id) noexcept
	{
		if (_on_construct_fn)
		{
			_on_construct_invoke_fn(_on_construct_fn, _storage, id);
		}
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

private:
	size_t			_capacity = 0;
	void*			_storage = nullptr;
	bool*			_mask = nullptr;
	entity_id*		_connector = nullptr;
	size_t*			_r_connector = nullptr;
	size_t			_occupied = 0;

	erase_fn_t		_erase_fn = nullptr;
	delete_fn_t		_delete_fn = nullptr;
	copy_fn_t		_copy_fn = nullptr;
	move_fn_t		_move_fn = nullptr;

	void*	_on_construct_fn = nullptr;
	void*	_on_destroy_fn = nullptr;

	handler_invoke_fn_t	_on_construct_invoke_fn = nullptr;
	handler_invoke_fn_t	_on_destroy_invoke_fn = nullptr;

	copy_handler_fn_t	_on_construct_fn_copy_fn = nullptr;
	copy_handler_fn_t	_on_destroy_fn_copy_fn = nullptr;

	delete_fn_t	 _on_construct_delete_fn = nullptr;
	delete_fn_t	 _on_destroy_delete_fn = nullptr;

	meta::type_info	_type_info;

	bool _populated = false;
};
}
}
}
#endif