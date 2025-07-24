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
	inline storage_manager(size_t storage_capacity, size_t capacity, const std::string& debug_name) noexcept
		: _debug_name(debug_name)
	{
		_capacity = capacity;
		_storage_capacity = storage_capacity;

		_storages = new poly_storage[storage_capacity]();
		_mask = new bool[storage_capacity]();
		_entries = new size_t[storage_capacity]();
		_r_entries = new size_t[storage_capacity]();
	};

	inline storage_manager(const storage_manager& other) noexcept
		: _debug_name(other._debug_name)
		, _capacity(other._capacity)
		, _storage_capacity(other._storage_capacity)
		, _entries_counter(other._entries_counter)			
	{
		_storages = new poly_storage[_storage_capacity];
		std::copy(other._storages, other._storages + _storage_capacity, _storages);

		_mask = new bool[_storage_capacity];
		std::copy(other._mask, other._mask + _storage_capacity, _mask);

		_entries = new size_t[_storage_capacity];
		std::copy(other._entries, other._entries + _storage_capacity, _entries);

		_r_entries = new size_t[_storage_capacity];
		std::copy(other._r_entries, other._r_entries + _storage_capacity, _r_entries);
	};

	inline storage_manager(storage_manager&& other) noexcept
		: _debug_name(std::move(other._debug_name))
		, _capacity(other._capacity)
		, _storage_capacity(other._storage_capacity)
		, _entries_counter(other._entries_counter)
		, _storages(other._storages)
		, _mask(other._mask)
		, _entries(other._entries)
		, _r_entries(other._r_entries)
	{
		other._storages = nullptr;
		other._mask = nullptr;
		other._entries = nullptr;
		other._r_entries = nullptr;
	};

	inline ~storage_manager() noexcept
	{
		delete[] _storages;
		delete[] _mask;
		delete[] _entries;
		delete[] _r_entries;
	};

	inline storage_manager& operator=(const storage_manager& other) noexcept
	{
		if (this != &other)
		{
			delete[] _storages;
			delete[] _mask;
			delete[] _entries;
			delete[] _r_entries;

			_debug_name = other._debug_name;
			_capacity = other._capacity;
			_storage_capacity = other._storage_capacity;
			_entries_counter = other._entries_counter;

			_storages = new poly_storage[_storage_capacity];
			std::copy(other._storages, other._storages + _storage_capacity, _storages);

			_mask = new bool[_storage_capacity];
			std::copy(other._mask, other._mask + _storage_capacity, _mask);

			_entries = new size_t[_storage_capacity];
			std::copy(other._entries, other._entries + _storage_capacity, _entries);

			_r_entries = new size_t[_storage_capacity];
			std::copy(other._r_entries, other._r_entries + _storage_capacity, _r_entries);
		}
		return *this;
	}

	inline storage_manager& operator=(storage_manager&& other) noexcept
	{
		if (this != &other)
		{
			delete[] _storages;
			delete[] _mask;
			delete[] _entries;
			delete[] _r_entries;
						
			_debug_name = std::move(other._debug_name);
			_capacity = other._capacity;
			_storage_capacity = other._storage_capacity;
			_entries_counter = other._entries_counter;
			_storages = other._storages;
			_mask = other._mask;
			_entries = other._entries;
			_r_entries = other._r_entries;

			other._storages = nullptr;
			other._mask = nullptr;
			other._entries = nullptr;
			other._r_entries = nullptr;
		}
		return *this;
	}

public:
	inline void clear() noexcept
	{
		std::fill(_mask, _mask + _storage_capacity, false);

		for (size_t i = 0; i < _entries_counter; i++)
		{
			_storages[_entries[i]].clear();
		}

		_entries_counter = 0;
		_entries_counter = 0;
	}

	template<typename T>
	inline storage_id get_id() noexcept
	{
		static storage_id id = _id_counter++;
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
		poly_storage& storage = get_storage<T>();

		return storage.emplace<T>(index, std::forward<Args>(args)...);
	}

	template<typename...Args>
	inline void erase(size_t index) noexcept
	{
		(([this, index]<typename T>()
		{
			
			get_storage<T>().erase(index);
		}.template operator() < Args > ()), ...);
	}

	template<typename...Args>
	inline bool has(size_t index) noexcept
	{
		return (([this, index]<typename T>()
		{
			return get_storage<T>().has(index);
		}.template operator()<Args>()) && ...);
	}

	template<typename T>
	inline T& get(size_t index) noexcept
	{
		return get_storage<T>().template get<T>(index);
	}

	template<typename T>
	inline T* get_if_has(size_t index) noexcept
	{
		return get_storage<T>().template get_if_has<T>(index);
	}

	template<typename...Args>
	inline void copy(size_t from, size_t to) noexcept
	{
		if (from != to)
		{
			(([this, from, to]<typename T>()
			{
				get_storage<T>().copy(from, to);

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
				get_storage<T>().move(from, to);

			}. template operator()<Args>()), ...);
		}
	}

	inline bool alive(storage_id e) noexcept
	{
		return _mask[e];
	}

	//inline void remove_unchecked(storage_id id) noexcept
	//{
	//	size_t idx = _r_entries[id];

	//	_entries[idx] = _entries[--_entries_counter];
	//	_r_entries[_entries[_entries_counter]] = idx;
	//}

	//inline void remove(storage_id id) noexcept
	//{
	//	bool& cell = _mask[id];

	//	if (cell)
	//	{
	//		size_t idx = _r_entries[id];

	//		_entries[idx] = _entries[--_entries_counter];
	//		_r_entries[_entries[_entries_counter]] = idx;
	//	}
	//}

	template<typename T>
	inline poly_storage& get_storage() noexcept
	{
		storage_id id = get_id<T>();

		poly_storage& storage = _storages[id];
		bool& cell = _mask[id];

		if (!cell)
		{
			storage.populate<T>(_capacity);
			cell = true;

			size_t idx = _entries_counter++;

			_entries[idx] = id;
			_r_entries[id] = idx;
		}

		return storage;
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
		return _entries + _entries_counter;
	}

	inline size_t occupied() noexcept
	{
		return _entries_counter;
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

