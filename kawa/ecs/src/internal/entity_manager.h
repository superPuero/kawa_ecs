#ifndef KAWA_ECS_ENTITY_MANAGER
#define KAWA_ECS_ENTITY_MANAGER

#include <limits>

namespace kawa
{
namespace ecs
{

typedef size_t entity_id;

constexpr inline entity_id nullent = std::numeric_limits<entity_id>::max();

class entity_manager
{
public:
	entity_manager(size_t capacity)
	{
		_capacity = capacity;

		_free_list				= new size_t[capacity]();
		_entity_mask			= new bool[capacity]();
		_entity_entries			= new size_t[capacity]();
		_r_entity_entries		= new size_t[capacity]();
	};
	~entity_manager()
	{
		delete[] _free_list;
		delete[] _entity_mask;
		delete[] _entity_entries;
		delete[] _r_entity_entries;

		_free_list = nullptr;
		_entity_mask = nullptr;
		_entity_entries = nullptr;
		_r_entity_entries = nullptr;
	};

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
		return _entity_mask[e];
	}

	inline void remove_unchecked(entity_id e) noexcept
	{
		_free_list[_free_list_size++] = e;
		_entity_mask[e] = false;

		size_t idx = _r_entity_entries[e];

		_entity_entries[idx] = _entity_entries[--_entries_counter];
		_r_entity_entries[_entity_entries[_entries_counter]] = idx;
	}


	inline void remove(entity_id e) noexcept
	{
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

	inline entity_id get_at_uncheked(size_t i) noexcept
	{
		return _entity_entries[i];
	}

	inline size_t* begin() noexcept 
	{
		return _entity_entries;
	}

	inline entity_id* end() noexcept
	{
		return _entity_entries + _occupied;
	}

	inline entity_id get_occupied() noexcept
	{
		return _occupied;
	}

private:
	bool* _entity_mask				= nullptr;

	entity_id* _entity_entries			= nullptr;
	size_t* _r_entity_entries		= nullptr;
	size_t	_entries_counter		= 0;

	size_t* _free_list				= nullptr;
	size_t	_free_list_size			= 0;

	size_t _occupied				= 0;
	size_t _capacity				= 0;

};
}
}

#endif 

