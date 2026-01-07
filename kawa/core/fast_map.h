#ifndef KAWA_FAST_MAP
#define KAWA_FAST_MAP

#include "core_types.h"
#include "indirect_array.h"

namespace kawa
{

	template<typename T>
	struct hash_map
	{
		struct config
		{
			usize capacity = 128;
			usize collision_depth = 8;
		};

		hash_map(const config& cfg)
			: _cfg(cfg)
			, values(cfg.capacity* cfg.collision_depth)
			, keys(cfg.capacity * cfg.collision_depth)
		{
			_capacity = _cfg.capacity * _cfg.collision_depth;
		}

		hash_map(const hash_map&) = default;
		hash_map(hash_map&&) = default;

		hash_map& operator=(const hash_map&) = default;
		hash_map& operator=(hash_map&&) = default;

		void clear() noexcept
		{
			keys.refresh(_capacity);
			values.refresh(_capacity);
		}

		template<typename...Args>
		T& insert(u64 hash_key, Args&&...args)
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					keys.emplace(index, hash_key);
					return values.emplace(index, std::forward<Args>(args)...);
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return values.emplace(index, std::forward<Args>(args)...);
					}
					else
					{
						index++;
					}
				}
			}

			kw_assert_msg(false, "hash_map collision depth exceeded");
		}

		bool contains(u64 hash_key)
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					return false;
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return true;
					}
					else
					{
						index++;
					}
				}
			}
		}

		void erase(u64 hash_key)
		{
			auto index = hash_key % _capacity;
			usize curr_collison = 0;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					return;
				}
				else
				{
					if (keys[index] == hash_key)
					{
						values.erase(index);
						keys.erase(index);
						return;
					}
					else
					{
						index++;
					}
				}
			}
		}

		T& get(u64 hash_key) noexcept
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					kw_assert(false);
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return values.get(index);
					}
					else
					{
						index++;
					}
				}
			}

			kw_assert(false);
		}

		const T& get(u64 hash_key) const noexcept
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					kw_assert(false);
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return values.get(index);
					}
					else
					{
						index++;
					}
				}
			}

			kw_assert(false);
		}

		T* try_get(u64 hash_key) noexcept
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					return nullptr;
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return &values.get(index);
					}
					else
					{
						index++;
					}
				}
			}

			return nullptr;
		}

		const T* try_get(u64 hash_key) const noexcept
		{
			usize index = hash_key % _capacity;

			for (usize i = 0; i < _cfg.collision_depth; i++)
			{
				if (!keys.contains(index))
				{
					return nullptr;
				}
				else
				{
					if (keys[index] == hash_key)
					{
						return &values.get(index);
					}
					else
					{
						index++;
					}
				}
			}

			return nullptr;
		}

		indirect_array<T> values;
		indirect_array<u64> keys;

	private:
		usize _capacity = 0;
		config _cfg;
	};

}

#endif // !KAWA_FAST_MAP
