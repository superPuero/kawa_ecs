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

	#define KW_ECS_ASSERT(expr) if(!(expr)) KW_ECS_DEBUG_BREAK();

    #define KW_ECS_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                std::cout << msg << '\n'; \
                KW_ECS_DEBUG_BREAK(); \
            } \
        } while(0)


#else
	#define KW_ECS_ASSERT(expr) ((void)0)
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
	namespace _internal
	{

		class poly_storage
		{
			using erase_fn_t = void(*)(void*, size_t);

		public:
			inline poly_storage() noexcept = default;
			inline ~poly_storage() noexcept
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
				KW_ECS_ASSERT(validate(index));

				return _mask[index];
			}

			template<typename T, typename...Args>
			inline T& emplace(size_t index, Args&&...args)  noexcept
			{
				KW_ECS_ASSERT(validate(index));

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
					erase(index);

					return *(new (static_cast<T*>(_storage) + index) T(std::forward<Args>(args)...));
				}

			}

			template<typename T>
			inline T& get(size_t index) noexcept
			{
				KW_ECS_ASSERT(validate(index));

				return *(static_cast<T*>(_storage) + index);
			}

			template<typename T>
			inline T* get_if_has(size_t index) noexcept
			{
				KW_ECS_ASSERT(validate(index));

				if (_mask[index])
				{
					return static_cast<T*>(_storage) + index;
				}
				return nullptr;
			}

			inline void erase(size_t index) noexcept
			{
				KW_ECS_ASSERT(validate(index));

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

			template<typename T>
			inline void copy(size_t from, size_t to) noexcept
			{
				KW_ECS_ASSERT(validate(from));
				KW_ECS_ASSERT(validate(to));

				if (has(from))
				{
					emplace<T>(to, get<T>(from));
				}
			}

			template<typename T>
			inline void move(size_t from, size_t to) noexcept
			{
				KW_ECS_ASSERT(validate(from));
				KW_ECS_ASSERT(validate(to));

				if (has(from))
				{
					emplace<T>(to, std::move(get<T>(from)));
					erase(from);
				}
			}

			inline bool validate(size_t id) noexcept
			{
				if (id >= _capacity)
				{
					std::cout << "out of bounds storage access" << '\n';
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
			size_t			_capacity = 0;

			bool			_popualted = false;

		};
	}

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
			_real_capacity			= capacity + 1;

			_storage			= new _internal::poly_storage[KW_MAX_UNIQUE_STORAGE_COUNT];
			_storage_mask		= new bool[KW_MAX_UNIQUE_STORAGE_COUNT]();
			_free_list			= new size_t[_real_capacity]();
			_entity_mask		= new bool[_real_capacity]();
			_entity_entries		= new size_t[_real_capacity]();
			_r_entity_entries	= new size_t[_real_capacity]();
		}

		inline ~registry() noexcept
		{
			for (storage_id i = 0; i < _storage_id_counter; i++)
			{
				if (_storage_mask[i])
				{
					_storage[i].~poly_storage();
				}
			}

			delete[] _storage;
			delete[] _storage_mask;
			delete[] _entity_mask;
			delete[] _free_list;
			delete[] _entity_entries;
			delete[] _r_entity_entries;
		}

		inline entity_id entity() noexcept
		{
			if (_free_list_size)
			{
				entity_id id = _free_list[--_free_list_size];
				_entity_mask[id] = true;

				return id;
			}

			if (_occupied >= _real_capacity)
			{
				return nullent;
			}

			entity_id id = _occupied++;
			_entity_mask[id] = true;

			_entity_entries[_entries_counter++] = id;
			_r_entity_entries[id] = _entries_counter;

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
			KW_ECS_ASSERT(validate_entity(entity));

			storage_id s_id = get_storage_id<T>();

			KW_ECS_ASSERT(validate_storage(s_id));

			bool& storage_cell = _storage_mask[s_id];

			_internal::poly_storage& storage = _storage[s_id];

			if(!storage_cell)
			{
				storage.popualte<T>(_real_capacity);
				storage_cell = true;
			}

			return storage.emplace<T>(entity, std::forward<Args>(args)...);
		}

		template<typename T>
		inline void erase(entity_id entity) noexcept
		{
			KW_ECS_ASSERT(validate_entity(entity));

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();
				_internal::poly_storage* storage = (_storage + s_id);

				if (_storage_mask[s_id])
				{
					storage->erase(entity);
				}
			}
		}

		template<typename T>
		inline bool has(entity_id entity) noexcept
		{
			KW_ECS_ASSERT(validate_entity(entity));

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT(validate_storage(s_id));

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
			KW_ECS_ASSERT(validate_entity(entity));

			storage_id s_id = get_storage_id<T>();

			KW_ECS_ASSERT(validate_storage(s_id));

			return _storage[s_id].get<T>(entity);
		}

		template<typename T>
		inline T* get_if_has(entity_id entity) noexcept
		{
			KW_ECS_ASSERT(validate_entity(entity));

			bool& entity_cell = _entity_mask[entity];

			if (entity_cell)
			{
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT(validate_storage(s_id));

				if (_storage_mask[s_id])
				{
					return _storage[s_id].get_if_has<T>(entity);
				}

				return nullptr;
			}

			return nullptr;
		}

		template<typename...Args>
		inline void copy(entity_id from, entity_id to) noexcept
		{
			KW_ECS_ASSERT(validate_entity(from));
			KW_ECS_ASSERT(validate_entity(to));

			(([&]<typename T>()
			{	
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT(validate_storage(s_id));

				_storage[s_id].copy<T>(from, to);

			}. template operator()<Args>()), ...);
		}

		template<typename...Args>
		inline void move(entity_id from, entity_id to) noexcept
		{
			KW_ECS_ASSERT(validate_entity(from));
			KW_ECS_ASSERT(validate_entity(to));

			(([&]<typename T>()
			{
				storage_id s_id = get_storage_id<T>();

				KW_ECS_ASSERT(validate_storage(s_id));

				_storage[s_id].move<T>(from, to);

			}. template operator() <Args> ()), ...);
		}

		inline void destroy(entity_id entity) noexcept
		{
			KW_ECS_ASSERT(validate_entity(entity));

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

				size_t idx = _r_entity_entries[entity];

				_entity_entries[idx] = _entity_entries[--_entries_counter];
				_r_entity_entries[_entity_entries[_entries_counter]] = idx;
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
					_query_opt_impl<Fn, opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
							std::forward<Params>(params)...
						);
				}
				else
				{
					_query_req_opt_impl<Fn, require_args_tuple, opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<require_args_tuple>>{},
							std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
							std::forward<Params>(params)...
						);
				}
			}
			else
			{
				_query_req_impl<Fn, require_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<std::tuple_size_v<require_args_tuple>>{},
						std::forward<Params>(params)...
					);
			}
		}

		template<typename Fn, typename...Params>
		inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
		{

			KW_ECS_ASSERT(validate_entity(entity));

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
						_query_with_opt_impl<Fn, opt_args_tuple>
							(
								entity,
								std::forward<Fn>(fn),
								std::make_index_sequence<std::tuple_size_v<opt_args_tuple>>{},
								std::forward<Params>(params)...
							);
					}
					else
					{
						_query_with_req_opt_impl<Fn, require_args_tuple, opt_args_tuple>
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
					_query_with_req_impl<Fn, require_args_tuple>
						(
							entity,
							std::forward<Fn>(fn),
							std::make_index_sequence<std::tuple_size_v<require_args_tuple>>{},
							std::forward<Params>(params)...
						);
				}
			}
		}

	private:
		template<typename Fn, typename rquire_tuple, size_t...require_idxs, typename...Params>
		inline void _query_req_impl(Fn&& fn, std::index_sequence<require_idxs...>, Params&&...params) noexcept
		{
			storage_id storage_keys[sizeof...(require_idxs)] =
			{ 
				get_storage_id<std::tuple_element_t<require_idxs, rquire_tuple>>()...,
			};

			if ((_storage_mask[storage_keys[require_idxs]] && ...))
			{
				_internal::poly_storage* storages[sizeof...(require_idxs)] = { &_storage[storage_keys[require_idxs]]... };

				_internal::poly_storage* smallest = storages[0];

				for (_internal::poly_storage* st : storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				for (size_t i = 0; i < smallest->_occupied; i++)
				{
					entity_id e = smallest->_connector[i];
																		
					if ((storages[require_idxs]->has(e) && ...))
					{
						fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, rquire_tuple>>(e)...);
					}
				}
			}
		}

		template<typename Fn, typename opt_tuple, size_t...opt_idxs, typename...Params>
		inline void _query_opt_impl(Fn&& fn, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			_internal::poly_storage* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

			for (size_t i = 0; i < _entries_counter; i++)
			{
				entity_id e = _entity_entries[i];
				fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
			}

		}

		template<typename Fn, typename require_tuple, typename opt_tuple, size_t...require_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_req_opt_impl(Fn&& fn, std::index_sequence<require_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id require_storage_keys[sizeof...(require_idxs)] =
			{
				get_storage_id<std::tuple_element_t<require_idxs, require_tuple>>()...,
			};

			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			if ((_storage_mask[require_storage_keys[require_idxs]] && ...))
			{
				_internal::poly_storage* storages[sizeof...(require_idxs)] = { &_storage[require_storage_keys[require_idxs]]... };
				_internal::poly_storage* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

				_internal::poly_storage* smallest = storages[0];

				for (_internal::poly_storage* st : storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				for (size_t i = 0; i < smallest->_occupied; i++)
				{
					entity_id e = smallest->_connector[i];
					if ((storages[require_idxs]->has(e) && ...))
					{
						fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, require_tuple>>(e)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
					}
				}
			}
		}

		template<typename Fn, typename require_tuple, size_t...require_idxs, typename...Params>
		inline void _query_with_req_impl(entity_id entity, Fn&& fn, std::index_sequence<require_idxs...>, Params&&...params) noexcept
		{
			storage_id require_storage_keys[sizeof...(require_idxs)] =
			{
				get_storage_id<std::tuple_element_t<require_idxs, require_tuple>>()...,
			};

			
			if ((_storage_mask[require_storage_keys[require_idxs]] && ...))
			{
				_internal::poly_storage* storages[sizeof...(require_idxs)] = { &_storage[require_storage_keys[require_idxs]]... };
				_internal::poly_storage* smallest = storages[0];

				for (_internal::poly_storage* st : storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				if (smallest->has(entity))
				{
					if ((storages[require_idxs]->has(entity) && ...))
					{
						fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, require_tuple>>(entity)...);
					}
				}
			}
		
		}

		template<typename Fn, typename opt_tuple, size_t...opt_idxs, typename...Params>
		inline void _query_with_opt_impl(entity_id entity, Fn&& fn, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			_internal::poly_storage* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

			fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);
		}

		template<typename Fn, typename require_tuple, typename opt_tuple, size_t...require_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_with_req_opt_impl(entity_id entity, Fn&& fn, std::index_sequence<require_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			storage_id require_storage_keys[sizeof...(require_idxs)] =
			{
				get_storage_id<std::tuple_element_t<require_idxs, require_tuple>>()...,
			};

			storage_id opt_storage_keys[sizeof...(opt_idxs)] =
			{
				get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
			};

			
			if ((_storage_mask[require_storage_keys[require_idxs]] && ...))
			{
				_internal::poly_storage* storages[sizeof...(require_idxs)] = { &_storage[require_storage_keys[require_idxs]]... };
				_internal::poly_storage* opt_storages[std::tuple_size_v<opt_tuple>] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

				_internal::poly_storage* smallest = storages[0];

				for (_internal::poly_storage* st : storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				if (smallest->has(entity))
				{
					if ((storages[require_idxs]->has(entity) && ...))
					{
						fn(std::forward<Params>(params)..., storages[require_idxs]->get<std::tuple_element_t<require_idxs, require_tuple>>(entity)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);
					}
				}
			}			
		}


	public:

		inline bool validate_entity(entity_id id) noexcept
		{
			if(id == nullent)
			{
				std::cout << "nullent entity usage" << '\n';
				return false;
			}
			else if (id >= _real_capacity)
			{
				std::cout << "out of bounds entity access" << '\n';
				return false;
			}

			return true;
		}

		inline bool validate_storage(storage_id id) noexcept
		{

			if (id >= KW_MAX_UNIQUE_STORAGE_COUNT)
			{
				std::cout << "invalid storage, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids" << '\n';
				return false;
			}

			return true;
		}
	
	private:
		_internal::poly_storage*	_storage			= nullptr;

		size_t						_capacity			= 0;
		size_t						_real_capacity		= 0;
		size_t						_occupied			= 1;

		bool*						_storage_mask		= nullptr;

		bool*						_entity_mask		= nullptr;

		size_t*						_entity_entries		= nullptr;
		size_t*						_r_entity_entries	= nullptr;
		size_t						_entries_counter	= 0;

		size_t*						_free_list			= nullptr;
		size_t						_free_list_size		= 0;

	private:
		static inline storage_id _storage_id_counter = 0;


	};
}
}

