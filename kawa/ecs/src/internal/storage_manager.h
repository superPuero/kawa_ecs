#ifndef KAWA_ECS_STORAGE_MANAGER
#define KAWA_ECS_STORAGE_MANAGER

#include <limits>
#include "poly_storage.h"
#include "meta.h"
#include "core.h"

namespace kawa
{
namespace ecs
{

using storage_id = size_t;

using component_info = meta::type_info;

namespace _
{
class storage_manager
{

public:
	storage_manager(size_t storage_capacity, size_t capacity, const std::string& debug_name)
		: _debug_name(debug_name)
	{
		_capacity = capacity;
		_storage_capacity = storage_capacity;

		_storages = new poly_storage[storage_capacity]();
		_mask = new bool[storage_capacity]();
		_entries = new size_t[storage_capacity]();
		_r_entries = new size_t[storage_capacity]();
	};

	~storage_manager()
	{
		delete[] _storages;
		delete[] _mask;
		delete[] _entries;
		delete[] _r_entries;
	};

public:
	template<typename T>
	inline storage_id get_id() noexcept
	{
		static storage_id id = register_id<T>();
		return id;
	}

	template<typename T>
	inline storage_id register_id() noexcept
	{
		storage_id id = _id_counter++;
	
		KAWA_DEBUG_EXPAND(_validate_storage(id));

		poly_storage& storage = _storages[id];
		storage.populate<T>(_capacity);

		_mask[id] = true;

		size_t idx = _entries_counter++;
		_entries[idx] = id;
		_r_entries[id] = idx;
		_occupied++;

		return id;	
	}

	template<typename...Args>
	inline void ensure() noexcept
	{
		(get_id<Args>(), ...);
	}

	template<typename T, typename...Args>
	inline T& emplace(size_t index, Args&&...args) noexcept
	{
		storage_id id = get_id<T>();

		bool& storage_cell = _mask[id];

		poly_storage& storage = _storages[id];

		if (!storage_cell)
		{
			storage.template populate<T>(_capacity);
			storage_cell = true;
		}

		return storage.emplace<T>(index, std::forward<Args>(args)...);
	}

	template<typename...Args>
	inline void erase(size_t index) noexcept
	{
		(([this, index]<typename T>()
		{
			storage_id id = get_id<T>();

			if (_mask[id])
			{
				_storages[id].erase(index);
			}
		}.template operator() < Args > ()), ...);
	}

	template<typename...Args>
	inline bool has(size_t index) noexcept
	{
		return (([this, index]<typename T>()
		{
			storage_id id = get_id<T>();

			if (_mask[id])
			{
				return _storages[id].has(index);
			}
			return false;
		}.template operator()<Args>()) && ...);
	}

	template<typename T>
	inline T& get(size_t index) noexcept
	{
		storage_id id = get_id<T>();

		if (_mask[id])
		{
			return _storages[id].get<T>(index);
		}

		KAWA_ASSERT(false);
	}

	template<typename T>
	inline T* get_if_has(size_t index) noexcept
	{
		storage_id id = get_id<T>();

		if (!_mask[id])
		{
			return nullptr;
		}

		return _storages[id].get_if_has<T>(index);
	}

	template<typename...Args>
	inline void copy(size_t from, size_t to) noexcept
	{
		if (from != to)
		{
			(([this, from, to]<typename T>()
			{
				storage_id id = get_id<T>();

				if (!_mask[id])
				{
					return;
				}

				_storages[id].copy(from, to);

			}. template operator()<Args>()), ...);
		}
	}

	template<typename...Args>
	inline void move(size_t from, size_t to) noexcept
	{
		if (from != to)
		{
			(([this, from, to]<typename T>()
			{
				storage_id id = get_id<T>();

				if (!_mask[id])
				{
					return;
				}

				_storages[id].move(from, to);

			}. template operator()<Args>()), ...);
		}
	}

	inline bool alive(storage_id e) noexcept
	{
		return _mask[e];
	}

	inline void remove_unchecked(storage_id id) noexcept
	{
		size_t idx = _r_entries[id];

		_entries[idx] = _entries[--_entries_counter];
		_r_entries[_entries[_entries_counter]] = idx;
	}

	inline void remove(storage_id id) noexcept
	{
		bool& cell = _mask[id];

		if (cell)
		{
			size_t idx = _r_entries[id];

			_entries[idx] = _entries[--_entries_counter];
			_r_entries[_entries[_entries_counter]] = idx;
		}
	}

	template<typename T>
	inline poly_storage& get_storage() noexcept
	{
		return _storages[get_id<T>()];
	}

	inline poly_storage& get_storage(storage_id id) noexcept
	{
		return _storages[id];
	}

	inline storage_id* begin() noexcept
	{
		return _entries;
	}

	inline storage_id* end() noexcept
	{
		return _entries + _occupied;
	}

	inline size_t occupied() noexcept
	{
		return _occupied;
	}

private:
	inline void _validate_storage(storage_id id) const noexcept
	{
		KAWA_ASSERT_MSG(id < _storage_capacity, "[ {} ]: maximum amoount of unique component types reached [ {} ], increase max_component_types", _debug_name, _storage_capacity);
	}

private:
	poly_storage*	_storages = nullptr;
	bool*			_mask = nullptr;

	storage_id*		_entries = nullptr;
	size_t*			_r_entries = nullptr;
	size_t			_entries_counter = 0;

	size_t			_occupied = 0;
	size_t			_capacity = 0;
	size_t			_storage_capacity = 0;

	std::string		_debug_name;

private:
	static inline size_t _id_counter = 0;
};
}
}
}

#endif 

