#ifndef KAWA_ECS_ENTITY_MANAGER
#define KAWA_ECS_ENTITY_MANAGER

#include <limits>
#include "ecs_types.h"
#include "../../../core/core.h"

namespace kawa
{
namespace ecs
{

constexpr inline entity_id nullent = std::numeric_limits<entity_id>::max();

namespace _
{
class entity_manager
{
public:
	entity_manager(size_t capacity, const std::string& debug_name)
		: _debug_name(debug_name)
	{
		_capacity = capacity;

		_free_list = new size_t[capacity]();
		_entity_mask = new bool[capacity]();
		_entity_entries = new size_t[capacity]();
		_r_entity_entries = new size_t[capacity]();

	};

	entity_manager(const entity_manager& other)
		: _debug_name(other._debug_name)
		, _capacity(other._capacity)
		, _occupied(other._occupied)
		, _free_list_size(other._free_list_size)
		, _entries_counter(other._entries_counter)
	{

		_free_list = new size_t[_capacity];
		std::copy(other._free_list, other._free_list + _capacity, _free_list);

		_entity_mask = new bool[_capacity];
		std::copy(other._entity_mask, other._entity_mask + _capacity, _entity_mask);

		_entity_entries = new size_t[_capacity];
		std::copy(other._entity_entries, other._entity_entries + _capacity, _entity_entries);

		_r_entity_entries = new size_t[_capacity];
		std::copy(other._r_entity_entries, other._r_entity_entries + _capacity, _r_entity_entries);
	};

	~entity_manager()
	{
		delete[] _free_list;
		delete[] _entity_mask;
		delete[] _entity_entries;
		delete[] _r_entity_entries;
	};

	inline entity_manager& operator=(const entity_manager& other)
	{
		if (this != &other)
		{
			delete[] _free_list;
			delete[] _entity_mask;
			delete[] _entity_entries;
			delete[] _r_entity_entries;


			_debug_name = other._debug_name;
			_capacity = other._capacity;
			_occupied = other._occupied;
			_free_list_size = other._free_list_size;
			_entries_counter = other._entries_counter;


			_free_list = new size_t[_capacity];
			std::copy(other._free_list, other._free_list + _capacity, _free_list);

			_entity_mask = new bool[_capacity];
			std::copy(other._entity_mask, other._entity_mask + _capacity, _entity_mask);

			_entity_entries = new size_t[_capacity];
			std::copy(other._entity_entries, other._entity_entries + _capacity, _entity_entries);

			_r_entity_entries = new size_t[_capacity];
			std::copy(other._r_entity_entries, other._r_entity_entries + _capacity, _r_entity_entries);
		}
		return *this;
	}

public:

	inline void clear() noexcept
	{
		std::fill(_entity_mask, _entity_mask + _capacity, false);
		_free_list_size = 0;
		_entries_counter = 0;
		_occupied = 0;
	}

	inline entity_id get_new() noexcept
	{
		entity_id id;
		if (_free_list_size)
		{
			id = _free_list[--_free_list_size];
		}
		else
		{
			if (_occupied >= _capacity)
			{
				return nullent;
			}
			id = _occupied++;
		}

		_entity_mask[id] = true;

		_entity_entries[_entries_counter] = id;
		_r_entity_entries[id] = _entries_counter;

		_entries_counter++;

		return id;
	}

	inline bool alive(entity_id e)	noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(e));

		return _entity_mask[e];
	}

	inline void remove_unchecked(entity_id e) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(e));

		_free_list[_free_list_size++] = e;
		_entity_mask[e] = false;

		size_t idx = _r_entity_entries[e];

		_entity_entries[idx] = _entity_entries[--_entries_counter];
		_r_entity_entries[_entity_entries[_entries_counter]] = idx;
	}


	inline void remove(entity_id e) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(e));

		bool& entity_cell = _entity_mask[e];

		if (entity_cell)
		{
			_free_list[_free_list_size++] = e;
			entity_cell = false;

			size_t idx = _r_entity_entries[e];

			_entity_entries[idx] = _entity_entries[--_entries_counter];
			_r_entity_entries[_entity_entries[_entries_counter]] = idx;
		}
	}

	inline entity_id get(size_t i) noexcept
	{
		KAWA_ASSERT_MSG(i < _occupied, "[ {} ]: index out of bounds [ {} ]", _debug_name, i);

		return _entity_entries[i];
	}

	inline entity_id* begin() noexcept
	{
		return _entity_entries;
	}

	inline entity_id* end() noexcept
	{
		return _entity_entries + _occupied;
	}

	inline size_t occupied() noexcept
	{
		return _occupied;
	}

	inline entity_id operator[](size_t i) noexcept
	{
		return get(i);
	}

private:
	inline void _validate_entity(entity_id id) const noexcept
	{
		KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", _debug_name);
		KAWA_ASSERT_MSG(id < _capacity, "[ {} ]: invalid entity_id [ {} ] usage", _debug_name, id);
	}

private:
	bool* _entity_mask = nullptr;

	entity_id* _entity_entries = nullptr;
	size_t* _r_entity_entries = nullptr;
	size_t	_entries_counter = 0;

	size_t* _free_list = nullptr;
	size_t	_free_list_size = 0;

	size_t _occupied = 0;
	size_t _capacity = 0;

	std::string _debug_name;
};
}
}
}

#endif 

