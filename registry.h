#pragma once

#include <memory>
#include <iostream>

#if defined(_DEBUG)

    #if defined(_MSC_VER)
        #include <intrin.h>
        #define KW_ECS_DEBUG_BREAK() __debugbreak()
    #elif defined(__GNUC__) || defined(__clang__)
        #define KW_ECS_DEBUG_BREAK() __builtin_trap()
    #else
        #define KW_ECS_DEBUG_BREAK() ((void)0)
    #endif

    #define KW_ECS_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                std::cout << msg << '\n'; \
                KW_ECS_DEBUG_BREAK(); \
            } \
        } while(0)

#else
    #define KW_ECS_ASSERT_MSG(expr, msg) ((void)0)
#endif

#define KW_MAX_UNIQUE_STORAGE_COUNT 255
			  
namespace kawa
{

namespace meta
{
	template<typename...Types>
	constexpr size_t get_ptr_type_count()
	{
		return (0 + ... + (std::is_pointer_v<Types> ? 1 : 0));
	}

	template<typename Tuple>
	constexpr size_t get_ptr_type_count_tuple()
	{
		return []<std::size_t... I>(std::index_sequence<I...>)
		{
			return get_ptr_type_count<std::tuple_element_t<I, Tuple>...>();
		}(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
	}

	template<std::size_t Start, std::size_t End>
	struct sub_tuple 
	{
		static_assert(Start <= End, "Start index must be <= End");

		template<typename Tuple>
		using of = decltype([]<std::size_t... I>(std::index_sequence<I...>) {
			return std::tuple<std::tuple_element_t<Start + I, Tuple>...>{};
		}(std::make_index_sequence<End - Start>{}));
	};

	template<typename tuple, size_t Offset, size_t...I>
	constexpr auto slice_tuple_impl(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<Offset + I, tuple>...> {};

	template<typename tuple>
	struct slice_tuple
	{
		template<size_t I>
		using at = decltype(slice_tuple_impl<tuple, I>(std::make_index_sequence<std::tuple_size_v<tuple> - I>{}));
	};

	template<typename RetTy, typename...ArgTy>
	struct function_traits {};

	template<typename RetTy, typename...ArgTy>
	struct function_traits<RetTy(*)(ArgTy...)>
	{
		using return_type = typename RetTy;
		using args_tuple = typename std::tuple<std::remove_reference_t<ArgTy>...>;
	};

	template<typename RetTy, typename...ArgTy>
	struct function_traits<RetTy(&)(ArgTy...)>
	{
		using return_type = typename RetTy;
		using args_tuple = typename std::tuple<std::remove_reference_t<ArgTy>...>;
	};

	template<typename T>
	struct function_traits<T> : function_traits<decltype(&T::operator())> {};

	template<typename RetTy, typename ObjTy, typename...ArgTy>
	struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
	{
		using return_type = typename RetTy;
		using args_tuple = typename std::tuple<std::remove_reference_t<ArgTy>...>;
	};
};

namespace ecs
{
	typedef size_t entity_id;
	typedef size_t storage_id;

	constexpr inline entity_id nullent = 0;

	class sparse_poly_set
	{
		using erase_fn_t = void(*)(void*, size_t);

	public:
		inline sparse_poly_set() noexcept = default;
		inline ~sparse_poly_set() noexcept
		{
			if (_popualted)
			{
				for (size_t i = 0; i < _occupied; i++)
				{
					size_t id = _connector[i];
					_erase_fn(_storage, id);
				}

				std::free(_storage);
											
				_popualted = false;
			}
		}

	public:
		template<typename T>
		inline void popualte(size_t capacity) noexcept
		{
			if (!_popualted)
			{
				_capacity = capacity;

				void* block = std::calloc(capacity, sizeof(T) + sizeof(bool) + sizeof(size_t) + sizeof(size_t));

				_storage		= block;
				_mask			= (bool*)(((T*)_storage) + capacity);
				_connector		= (size_t*)(((bool*)_mask) + capacity);
				_r_connector	= (size_t*)(((size_t*)_connector) + capacity);

				_erase_fn =
					[](void* data, size_t index)
					{
						(static_cast<T*>(data) + index)->~T();
					};

				_popualted = true;
			}
		}
	public:
		inline bool has(size_t index) noexcept
		{
			KW_ECS_ASSERT_MSG(validate(index), "index out of boudns");

			return _mask[index];
		}

		template<typename T, typename...Args>
		inline T& emplace(size_t index, Args&&...args)  noexcept
		{
			KW_ECS_ASSERT_MSG(validate(index), "index out of boudns");

			bool& cell = _mask[index];
			if (!cell)
			{
				size_t l_idx = _occupied++;

				_connector[l_idx] = index;
				_r_connector[index] = l_idx;

				cell = true;

				return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
			}

			return *(static_cast<T*>(_storage) + index);

		}

		template<typename T>
		inline T& get(size_t index) noexcept
		{
			KW_ECS_ASSERT_MSG(validate(index), "index out of boudns");

			return *(static_cast<T*>(_storage) + index);
		}

		template<typename T>
		inline T* get_if_has(size_t index) noexcept
		{
			KW_ECS_ASSERT_MSG(validate(index), "index out of boudns");

			if (_mask[index])
			{
				return static_cast<T*>(_storage) + index;
			}
			return nullptr;
		}

		inline void erase(size_t index) noexcept
		{
			KW_ECS_ASSERT_MSG(validate(index), "index out of boudns");

			bool& cell = _mask[index];
			if (cell)
			{
				size_t l_idx = _r_connector[index];

				_erase_fn(_storage, index);

				_connector[l_idx] = _connector[--_occupied];
				_r_connector[_connector[_occupied]] = _r_connector[index];

				cell = false;
			}
		}

		inline bool validate(size_t index) noexcept
		{
			return index < _capacity;
		}

	//private:
		void*			_storage = nullptr;
		bool*			_mask = nullptr;
		size_t*			_connector = nullptr;
		size_t*			_r_connector = nullptr;
		size_t			_occupied = 0;

		erase_fn_t		_erase_fn = nullptr;
		size_t			_capacity = 0;

		bool			_popualted = false;

	};

	class registry
	{
		template<typename T>
		static inline storage_id get_storage_id() noexcept
		{
			static storage_id id = _storage_id_counter++;
			return id;
		}

	public:
		inline registry(size_t capacity) noexcept
		{
			_capacity			= capacity;
			_r_capacity			= capacity + 1;

			_storage			= new sparse_poly_set[KW_MAX_UNIQUE_STORAGE_COUNT];
			_storage_mask		= new bool[KW_MAX_UNIQUE_STORAGE_COUNT]();
			_free_list			= new size_t[_r_capacity]();
			_entity_mask		= new bool[_r_capacity]();
		}

		inline ~registry() noexcept
		{
			for (storage_id i = 0; i < _storage_id_counter; i++)
			{
				if (_storage_mask[i])
				{
					_storage[i].~sparse_poly_set();
				}
			}

			delete[] _storage;
			delete[] _storage_mask;
			delete[] _entity_mask;
			delete[] _free_list;
		}

		inline entity_id entity() noexcept
		{
			if (_free_list_size)
			{
				entity_id id = _free_list[--_free_list_size];
				_entity_mask[id] = true;

				return id;
			}

			if (_occupied >= _r_capacity)
			{
				return nullent;
			}

			entity_id id = _occupied++;
			_entity_mask[id] = true;

			return id;
		}


		template<typename...Args>
		inline entity_id entity_with(Args&&...args) noexcept
		{		
			entity_id id = entity();

			(emplace<Args>(id, std::forward<Args>(args)), ...);

			return id;
		}

		template<typename T, typename...Args>
		inline T& emplace(entity_id entity, Args&&...args) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			storage_id s_id = get_storage_id<T>();

			KW_ECS_ASSERT_MSG(validate_storage(s_id), "invalid storage, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");

			bool& storage_cell = _storage_mask[s_id];

			sparse_poly_set& storage = _storage[s_id];

			if(!storage_cell)
			{
				storage.popualte<T>(_r_capacity);
				storage_cell = true;
			}

			return storage.emplace<T>(entity, std::forward<Args>(args)...);
		}

		template<typename T>
		inline void erase(entity_id entity) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();
				sparse_poly_set* storage = (_storage + s_id);

				if (_storage_mask[s_id])
				{
					storage->erase(entity);
				}
			}
		}

		template<typename T>
		inline bool has(entity_id entity) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT_MSG(validate_storage(s_id), "invalid storage, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");

				if (_storage_mask[s_id])
				{
					return _storage[s_id].has(entity);
				}

				return false;
			}
	
			return false;
		}

		template<typename T>
		inline T& get(entity_id entity) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			storage_id s_id = get_storage_id<T>();

			KW_ECS_ASSERT_MSG(validate_storage(s_id), "invalid storage, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");

			return _storage[s_id].get<T>(entity);
		}

		template<typename T>
		inline T* get_if_has(entity_id entity) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT_MSG(validate_storage(s_id), "invalid storage, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");

				if (_storage_mask[s_id])
				{
					return _storage[s_id].get_if_has<T>(entity);
				}

				return nullptr;
			}

			return nullptr;
		}

		inline void destroy(entity_id entity) noexcept
		{
			KW_ECS_ASSERT_MSG(validate_entity(entity), "entity id out of bounds");

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				for (size_t s_id = 0; s_id < _storage_id_counter; s_id++)
				{
					if(_storage_mask[s_id])
					{ 
						_storage[s_id].erase(entity);
					}
				}

				_free_list[_free_list_size++] = entity;
				entity_cell = false;
			}
		}

		template<typename Fn, typename...Params>
		inline void query(Fn&& fn, Params&&...params) noexcept
		{
			constexpr size_t params_count = sizeof...(Params);

			using args_tuple = meta::function_traits<Fn>::args_tuple;
			constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> - opt_args_count>::template of<no_params_args_tuple>;
			constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> - opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;

			if constexpr (args_count == params_count)
			{
				fn(std::forward<Params>(params)...);
			}
			else if constexpr (opt_args_count)
			{
				if constexpr (!require_args_count)
				{
					query_opt_impl<Fn, opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
							std::forward<Params>(params)...
						);
				}
				else
				{
					query_req_opt_impl<Fn, require_args_tuple, opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<require_args_tuple> -1>{},
							std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
							std::forward<Params>(params)...
						);
				}
			}
			else
			{
				query_req_impl<Fn, require_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<std::tuple_size_v<require_args_tuple> -1>{},
						std::forward<Params>(params)...
					);
			}
		}

		template<typename Fn, typename...Params>
		inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
		{
			constexpr size_t params_count = sizeof...(Params);

			using args_tuple = meta::function_traits<Fn>::args_tuple;
			constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
			if (_entity_mask[entity])
			{
				if constexpr (args_count == params_count)
				{
					fn(std::forward<Params>(params)...);
				}
				else if constexpr (opt_args_count)
				{
					if constexpr (!require_args_count)
					{
						query_with_opt_impl<Fn, opt_args_tuple>
							(
								entity,
								std::forward<Fn>(fn),
								std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
								std::forward<Params>(params)...
							);
					}
					else
					{
						query_with_req_opt_impl<Fn, require_args_tuple, opt_args_tuple>
							(
								entity,
								std::forward<Fn>(fn),
								std::make_index_sequence<std::tuple_size_v<require_args_tuple> -1>{},
								std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
								std::forward<Params>(params)...
							);
					}
				}
				else
				{
					query_with_req_impl<Fn, require_args_tuple>
						(
							entity,
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<require_args_tuple> -1>{},
							std::forward<Params>(params)...
						);
				}
			}
		}

	private:
		template<typename Fn, typename rquire_tuple, size_t...require_idxs, typename...Params>
		inline void query_req_impl(Fn&& fn, std::index_sequence<require_idxs...>, Params&&...params) noexcept
		{
			storage_id storage_keys[sizeof...(require_idxs) + 1] =
			{ 
				get_storage_id<std::tuple_element_t<require_idxs, rquire_tuple>>()...,
				get_storage_id<std::tuple_element_t<sizeof...(require_idxs), rquire_tuple>>()
			};

			if (_storage_mask[storage_keys[sizeof...(require_idxs)]])
			{
				if ((_storage_mask[storage_keys[require_idxs]] && ...))
				{
					sparse_poly_set* storages[std::tuple_size_v<rquire_tuple> + 1] = { &_storage[storage_keys[require_idxs]]..., &_storage[storage_keys[sizeof...(require_idxs)]] };
					sparse_poly_set& last = *storages[sizeof...(require_idxs)];

					for (size_t i = 0; i < last._occupied; i++)
					{
						entity_id e = last._connector[i];
																		
						if ((storages[require_idxs]->has(e) && ...))
						{
							fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, rquire_tuple>>(e)..., last.get<std::tuple_element_t<sizeof...(require_idxs), rquire_tuple>>(e));
						}
					}
				}
			}
		}

		template<typename Fn, typename opt_tuple, size_t...opt_idxs, typename...Params>
		inline void query_opt_impl(Fn&& fn, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{

			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			sparse_poly_set* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

			for (size_t i = 0; i < _occupied; i++)
			{
				if(_entity_mask[i])
				{
					fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(i) : nullptr)...);
				}
			}

		}

		template<typename Fn, typename require_tuple, typename opt_tuple, size_t...require_idxs, size_t...opt_idxs, typename...Params>
		inline void query_req_opt_impl(Fn&& fn, std::index_sequence<require_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id require_storage_keys[sizeof...(require_idxs) + 1] =
			{
				get_storage_id<std::tuple_element_t<require_idxs, require_tuple>>()...,
				get_storage_id<std::tuple_element_t<sizeof...(require_idxs), require_tuple>>()
			};

			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			if (_storage_mask[require_storage_keys[sizeof...(require_idxs)]])
			{
				if ((_storage_mask[require_storage_keys[require_idxs]] && ...))
				{
					sparse_poly_set* storages[std::tuple_size_v<require_tuple> +1] = { &_storage[require_storage_keys[require_idxs]]..., &_storage[require_storage_keys[sizeof...(require_idxs)]] };
					sparse_poly_set* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };						
					sparse_poly_set& last = *storages[sizeof...(require_idxs)];

					for (size_t i = 0; i < last._occupied; i++)
					{
						entity_id e = last._connector[i];
						fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, require_tuple>>(e)..., last.get<std::tuple_element_t<sizeof...(require_idxs), require_tuple>>(e), (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
					}
				}
			}
		}

		template<typename Fn, typename require_tuple, size_t...requaire_idxs, typename...Params>
		inline void query_with_req_impl(entity_id entity, Fn&& fn, std::index_sequence<requaire_idxs...>, Params&&...params) noexcept
		{
			storage_id storage_keys[sizeof...(requaire_idxs) + 1] =
			{
				get_storage_id<std::tuple_element_t<requaire_idxs, require_tuple>>()...,
				get_storage_id<std::tuple_element_t<sizeof...(requaire_idxs), require_tuple>>()
			};

			if (_storage_mask[storage_keys[sizeof...(requaire_idxs)]])
			{
				if ((_storage_mask[storage_keys[requaire_idxs]] && ...))
				{
					sparse_poly_set* storages[std::tuple_size_v<require_tuple> + 1] = { &_storage[storage_keys[requaire_idxs]]..., &_storage[storage_keys[sizeof...(requaire_idxs)]] };
					sparse_poly_set& last = *storages[sizeof...(requaire_idxs)];

					if (last.has(entity))
					{
						if ((storages[requaire_idxs]->has(entity) && ...))
						{
							fn(std::forward<Params>(params)..., storages[requaire_idxs]->get<std::tuple_element_t<requaire_idxs, require_tuple>>(entity)..., last.get<std::tuple_element_t<sizeof...(requaire_idxs), require_tuple>>(entity));
						}
					}
				}
			}
		}

		template<typename Fn, typename opt_tuple, size_t...opt_idxs, typename...Params>
		inline void query_with_opt_impl(entity_id entity, Fn&& fn, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			sparse_poly_set* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

			fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);
		}

		template<typename Fn, typename require_tuple, typename opt_tuple, size_t...require_idxs, size_t...opt_idxs, typename...Params>
		inline void query_with_req_opt_impl(entity_id entity, Fn&& fn, std::index_sequence<require_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id require_storage_keys[sizeof...(require_idxs) + 1] =
			{
				get_storage_id<std::tuple_element_t<require_idxs, require_tuple>>()...,
				get_storage_id<std::tuple_element_t<sizeof...(require_idxs), require_tuple>>()
			};

			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			if (_storage_mask[require_storage_keys[sizeof...(require_idxs)]])
			{
				if ((_storage_mask[require_storage_keys[require_idxs]] && ...))
				{
					sparse_poly_set* storages[std::tuple_size_v<require_tuple> + 1] = { &_storage[require_storage_keys[require_idxs]]..., &_storage[require_storage_keys[sizeof...(require_idxs)]] };
					sparse_poly_set& last = *storages[sizeof...(require_idxs)];
					sparse_poly_set* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					if (last.has(entity))
					{
						if ((storages[require_idxs]->has(entity) && ...))
						{
							fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, require_tuple>>(entity)..., last.get<std::tuple_element_t<sizeof...(require_idxs), require_tuple>>(entity), (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);
						}
					}
				}
			}
		}


	public:

		inline bool validate_entity(entity_id id) noexcept
		{
			return id < _r_capacity;
		}

		inline bool validate_storage(storage_id id) noexcept
		{
			return id < KW_MAX_UNIQUE_STORAGE_COUNT;
		}
	
	private:
		sparse_poly_set*	_storage				= nullptr;

		size_t				_capacity				= 0;
		size_t				_r_capacity				= 0;
		size_t				_occupied				= 1;

		bool*				_storage_mask			= nullptr;

		bool*				_entity_mask			= nullptr;
		size_t*				_free_list				= nullptr;
		size_t				_free_list_size			= 0;

	private:
		static inline storage_id _storage_id_counter = 0;


	};
}
}

