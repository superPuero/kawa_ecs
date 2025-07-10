#pragma once

#include <memory>
#include <iostream>
#include <tuple>
#include <type_traits>
#include <thread>
#include <functional>
#include <barrier>
#include <utility>
#include <cstdint>
#include <cstddef>

#ifdef _DEBUG																			

#ifdef _MSC_VER
#include <intrin.h>
#define KW_ECS_DEBUG_BREAK() __debugbreak()
#elif __GNUC__ || __clang__
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

#ifndef KAWA_ECS_PARALLELISM
	#define KAWA_ECS_PARALLELISM (std::thread::hardware_concurrency() / 2)
#endif

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
			return[]<size_t... I>(std::index_sequence<I...>)
			{
				return get_ptr_type_count<std::tuple_element_t<I, Tuple>...>();
			}(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
		}

		template<size_t Start, size_t End>
		struct sub_tuple
		{
			static_assert(Start <= End, "Start index must be <= End");

			template<typename Tuple>
			using of = decltype([]<size_t... I>(std::index_sequence<I...>)
			{
				return std::tuple<std::tuple_element_t<Start + I, Tuple>...>{};
			}(std::make_index_sequence<End - Start>{}));
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits {};
		
		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(*)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<std::remove_const_t<std::remove_reference_t<ArgTy>>...>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(&)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple< std::remove_const_t<std::remove_reference_t<ArgTy>>...>;
		};

		template<typename T>
		struct function_traits<T> : function_traits<decltype(&T::operator())> {};

		template<typename RetTy, typename ObjTy, typename...ArgTy>
		struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple< std::remove_const_t<std::remove_reference_t<ArgTy>>...>;
		};
		
		template<typename Fn, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params);

			using args_tuple = typename meta::function_traits<Fn>::args_tuple;
			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = typename meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = typename meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = typename  meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using args_tuple = meta::function_traits<Fn>::args_tuple;
			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> - opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
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
				using delete_fn_t	= void(*)(void*);
				using erase_fn_t	= void(*)(void*, size_t);
				using copy_fn_t		= void(*)(void*, size_t, size_t);
				using move_fn_t		= void(*)(void*, size_t, size_t);
				
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

						_storage = ::operator new(sizeof(T) * capacity, std::align_val_t{ alignof(T) });

						_delete_fn =
							[](void* data)
							{
								::operator delete(data, std::align_val_t{ alignof(T) });
							};

						_mask =			new bool[capacity]();
						_connector =	new size_t[capacity]();
						_r_connector =	new size_t[capacity]();

						_erase_fn =
							[](void* data, size_t index)
							{
								(static_cast<T*>(data) + index)->~T();
							};

						_copy_fn = [](void* data, size_t from, size_t to) 
							{
								if constexpr (std::is_trivially_copyable_v<T>)
								{
									memcpy
									(
										static_cast<T*>(data) + to, 
										static_cast<T*>(data) + from, 
										sizeof(T)
									);

								}
								else if constexpr (std::is_copy_constructible_v<T>) 
								{
									new (static_cast<T*>(data) + to) T(*(static_cast<T*>(data) + from));
								}
								else 
								{
									KW_ECS_ASSERT_MSG(false, std::string("trying to copy uncopyable type") + typeid(T).name());
								}
							};

						_move_fn = [](void* data, size_t from, size_t to)
							{
								if constexpr (std::is_trivially_move_constructible_v<T>)
								{
									memcpy
									(
										static_cast<T*>(data) + to,
										static_cast<T*>(data) + from,
										sizeof(T)
									);
								}
								else if constexpr (std::is_move_constructible_v<T>)
								{
									new (static_cast<T*>(data) + to) T(std::move(*(static_cast<T*>(data) + from)));
								}
								else
								{
									KW_ECS_ASSERT_MSG(false, std::string("trying to move unmovable type") + typeid(T).name());
								}
							};


						_populated = true;
					}
				}
			public:
				inline bool has(size_t index) noexcept
				{
					KW_ECS_ASSERT(_validate_index(index));

					return _mask[index];
				}

				template<typename T, typename...Args>
				inline T& emplace(size_t index, Args&&...args)  noexcept
				{
					KW_ECS_ASSERT(_validate_index(index));

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
					KW_ECS_ASSERT(_validate_index(index));

					return *(static_cast<T*>(_storage) + index);
				}

				template<typename T>
				inline T* get_if_has(size_t index) noexcept
				{
					KW_ECS_ASSERT(_validate_index(index));

					if (_mask[index])
					{
						return static_cast<T*>(_storage) + index;
					}
					return nullptr;
				}

				inline void erase(size_t index) noexcept
				{
					KW_ECS_ASSERT(_validate_index(index));

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
					KW_ECS_ASSERT(_validate_index(from));
					KW_ECS_ASSERT(_validate_index(to));

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
					KW_ECS_ASSERT(_validate_index(from));
					KW_ECS_ASSERT(_validate_index(to));

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

			};

			class query_par_engine
			{
			public:
				query_par_engine(size_t thread_count)
					: _barrier(thread_count + 1)
					, _thread_count(thread_count)
					, _tasks_count(thread_count + 1)
				{
					_starts = new size_t[_tasks_count]();
					_ends	= new size_t[_tasks_count]();

					if (_thread_count)
					{
						_threads = (std::thread*)::operator new(sizeof(std::thread) * _thread_count, std::align_val_t{ alignof(std::thread) });

						for (size_t i = 0; i < _thread_count; i++)
						{
							new (_threads + i) std::thread
							(
								[this, i]()
								{
									while (true)
									{
										_barrier.arrive_and_wait();

										if (_should_join)
										{
											_barrier.arrive_and_wait();
											return;
										}

										_query(_starts[i], _ends[i]);

										_barrier.arrive_and_wait();
									}
								}
							);
						}
					}
				}
				~query_par_engine()
				{
					_should_join = true;

					_barrier.arrive_and_wait();

					_barrier.arrive_and_wait();

					for (size_t i = 0; i < _thread_count; i++)
					{
						_threads[i].join();
					}

					if (_thread_count)
					{
						::operator delete(_threads, std::align_val_t{ alignof(std::thread) });
					}

					delete[] _ends;
					delete[] _starts;
				}

			public:
				template<typename Fn>
				void query(Fn&& query, size_t work)
				{
					_query = std::forward<Fn>(query);

					size_t chunk_work = work / _tasks_count;

					size_t tail_chunk_work = work - chunk_work * _tasks_count;

					for (size_t i = 0; i < _tasks_count; ++i)
					{
						size_t start = i * chunk_work;
						size_t end = (i == (_tasks_count - 1)) ? start + chunk_work + tail_chunk_work : start + chunk_work;

						if (start >= end) continue;

						_starts[i] = start;
						_ends[i] = end;
					}

					_barrier.arrive_and_wait();

					_query(_starts[_tasks_count - 1], _ends[_tasks_count - 1]);

					_barrier.arrive_and_wait();
				}

			private:
				std::thread*							_threads		= nullptr;
				const size_t							_thread_count	= 0;
				const size_t							_tasks_count	= 0;
				bool									_should_join	= false;

				size_t*									_starts			= nullptr;
				size_t*									_ends			= nullptr;

				std::function<void(size_t, size_t)>		_query			= nullptr;
				std::barrier<>							_barrier;

			};
		}

		class registry
		{
			template<typename T>
			static inline storage_id get_storage_id() noexcept
			{
				static storage_id id = _storage_id_counter++;
				KW_ECS_ASSERT_MSG(id < KW_MAX_UNIQUE_STORAGE_COUNT, "max amoount os storage ids reached, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");
				return id;
			}

		public:
			inline registry(size_t capacity) noexcept
				: _query_par_engine(KAWA_ECS_PARALLELISM)
			{
				_capacity = capacity;
				_real_capacity = capacity + 1;

				_storage = new _internal::poly_storage[KW_MAX_UNIQUE_STORAGE_COUNT];
				_storage_mask = new bool[KW_MAX_UNIQUE_STORAGE_COUNT]();
				_free_list = new size_t[_real_capacity]();
				_entity_mask = new bool[_real_capacity]();
				_entity_entries = new size_t[_real_capacity]();
				_r_entity_entries = new size_t[_real_capacity]();
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
				KW_ECS_ASSERT(_validate_entity(entity));

				static_assert(!std::is_const_v<T>, "component can not have const qualifier");

				storage_id s_id = get_storage_id<T>();

				bool& storage_cell = _storage_mask[s_id];

				_internal::poly_storage& storage = _storage[s_id];

				if (!storage_cell)
				{
					storage.populate<T>(_real_capacity);
					storage_cell = true;
				}

				return storage.emplace<T>(entity, std::forward<Args>(args)...);
			}

			template<typename T>
			inline void erase(entity_id entity) noexcept
			{
				KW_ECS_ASSERT(_validate_entity(entity));

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
				KW_ECS_ASSERT(_validate_entity(entity));

				bool& entity_cell = _entity_mask[entity];

				if (entity_cell)
				{
					storage_id s_id = get_storage_id<T>();

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
				KW_ECS_ASSERT(_validate_entity(entity));

				storage_id s_id = get_storage_id<T>();

				return _storage[s_id].get<T>(entity);
			}

			template<typename T>
			inline T* get_if_has(entity_id entity) noexcept
			{
				KW_ECS_ASSERT(_validate_entity(entity));

				bool& entity_cell = _entity_mask[entity];

				if (entity_cell)
				{
					storage_id s_id = get_storage_id<T>();

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
					KW_ECS_ASSERT(_validate_entity(from));
					KW_ECS_ASSERT(_validate_entity(to));

					if (from != to)
					{
						(([&]<typename T>()
						{
							storage_id s_id = get_storage_id<T>();
							_storage[s_id].copy(from, to);

						}. template operator() < Args > ()), ...);
					}
				}

				template<typename...Args>
				inline void move(entity_id from, entity_id to) noexcept
				{
					KW_ECS_ASSERT(_validate_entity(from));
					KW_ECS_ASSERT(_validate_entity(to));

					if (from != to)
					{
						(([&]<typename T>()
						{
							storage_id s_id = get_storage_id<T>();

							_storage[s_id].move(from, to);

						}. template operator() < Args > ()), ...);
					}
				}

				inline entity_id clone(entity_id from) noexcept
				{
					KW_ECS_ASSERT(_validate_entity(from));

					entity_id e = entity();

					if (!e) return e;

					for(storage_id s_id = 0; s_id < KW_MAX_UNIQUE_STORAGE_COUNT; s_id++)
					{
						if (_storage_mask[s_id])
						{
							_storage[s_id].copy(from, e);
						}
					}

					return e;
				}

				inline void clone(entity_id from, entity_id to) noexcept
				{
					KW_ECS_ASSERT(_validate_entity(from));
					KW_ECS_ASSERT(_validate_entity(to));

					for (storage_id s_id = 0; s_id < KW_MAX_UNIQUE_STORAGE_COUNT; s_id++)
					{
						if (_storage_mask[s_id])
						{
							_storage[s_id].copy(from, to);
						}
					}
				}

			inline void destroy(entity_id entity) noexcept
			{
				KW_ECS_ASSERT(_validate_entity(entity));

				bool& entity_cell = _entity_mask[entity];

				if (entity_cell)
				{
					for (size_t s_id = 0; s_id < _storage_id_counter; s_id++)
					{
						if (_storage_mask[s_id])
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
				using query_traits = typename meta::query_traits<Fn, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					for (size_t i = 0; i < _entries_counter; i++)
					{
						fn(std::forward<Params>(params)...);
					}
				}
				else
				{
					_query_impl<Fn, typename query_traits::require_args_tuple, typename query_traits::opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::require_args_count>{},
							std::make_index_sequence<query_traits::opt_args_count>{},
							std::forward<Params>(params)...
						);
				}
			}

			template<typename Fn, typename...Params>
			inline void query_par(Fn&& fn, Params&&...params) noexcept
			{
				using query_traits = typename meta::query_traits<Fn, Params...>;

				if constexpr (query_traits::args_count == query_traits::params_count)
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; i++)
							{ 
								fn(std::forward<Params>(params)...);
							}
						}
						, _entries_counter
					);
				}
				else
				{
					_query_par_impl<Fn, typename query_traits::require_args_tuple, typename query_traits::opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::require_args_count>{},
							std::make_index_sequence<query_traits::opt_args_count>{},
							std::forward<Params>(params)...
						);
				}
			}

			template<typename Fn, typename...Params>
			inline void query_self(Fn&& fn, Params&&...params) noexcept
			{
				using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

				static_assert(std::is_same_v<std::tuple_element_t<0, typename query_self_traits::args_tuple>, kawa::ecs::entity_id>, "query self fucntion must take kawa::ecs::entity_id as a first parameter");

				if constexpr (query_self_traits::args_count == query_self_traits::params_count)
				{
					for (size_t i = 0; i < _entries_counter; i++)
					{
						fn(_entity_entries[i], std::forward<Params>(params)...);
					}
				}
				else
				{
					_query_self_impl<Fn, typename query_self_traits::require_args_tuple, typename query_self_traits::opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_self_traits::require_args_count>{},
							std::make_index_sequence<query_self_traits::opt_args_count>{},
							std::forward<Params>(params)...
						);
				}
			}

			template<typename Fn, typename...Params>
			inline void query_self_par(Fn&& fn, Params&&...params) noexcept
			{
				using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

				static_assert(std::is_same_v<std::tuple_element_t<0, typename query_self_traits::args_tuple>, kawa::ecs::entity_id>, "query self fucntion must take kawa::ecs::entity_id as a first parameter");

				if constexpr (query_self_traits::args_count == query_self_traits::params_count)
				{
					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; i++)
							{ 

								fn(_entity_entries[i], std::forward<Params>(params)...);
							}
						}
						, _entries_counter
					);
				}
				else
				{
					_query_self_par_impl<Fn, typename  query_self_traits::require_args_tuple, typename  query_self_traits::opt_args_tuple>
						(
							std::forward<Fn>(fn),
							std::make_index_sequence<query_self_traits::require_args_count>{},
							std::make_index_sequence<query_self_traits::opt_args_count>{},
							std::forward<Params>(params)...
						);
				}
			}

			template<typename Fn, typename...Params>
			inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
			{
				KW_ECS_ASSERT(_validate_entity(entity));

				using query_traits = typename meta::query_traits<Fn, Params...>;

				if (_entity_mask[entity])
				{
					if constexpr (query_traits::args_count == query_traits::params_count)
					{
						fn(std::forward<Params>(params)...);
					}
					else
					{
						_query_with_impl<Fn, typename query_traits::require_args_tuple, typename query_traits::opt_args_tuple>
							(
								entity,
								std::forward<Fn>(fn),
								std::make_index_sequence<query_traits::require_args_count>{},
								std::make_index_sequence<query_traits::opt_args_count>{},
								std::forward<Params>(params)...
							);
					}
				}
			}

		private:
			template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t req_count = sizeof...(req_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);

				if constexpr (req_count && opt_count)
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};


					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };
					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					for (size_t i = 0; i < smallest->_occupied; i++)
					{
						entity_id e = smallest->_connector[i];
						if ((req_storages[req_idxs]->has(e) && ...))
						{
							fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
						}
					}
				}

				else if constexpr (!req_count)
				{
					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					for (size_t i = 0; i < _entries_counter; i++)
					{
						entity_id e = _entity_entries[i];
						fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
					}
				}
				else
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};


					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					for (size_t i = 0; i < smallest->_occupied; i++)
					{
						entity_id e = smallest->_connector[i];
						if ((req_storages[req_idxs]->has(e) && ...))
						{
							fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...);
						}
					}
				}
			}

			
			template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_par_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t req_count = sizeof...(req_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);

				if constexpr (req_count && opt_count)
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};


					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };
					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->_connector[i];
								if ((req_storages[req_idxs]->has(e) && ...))
								{
									fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
								}
							}
						}
						, smallest->_occupied
					);
				}

				else if constexpr (!req_count)
				{
					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = _entity_entries[i];
								fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
							}
						}
						, _entries_counter
					);

				}
				else
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};


					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->_connector[i];
								if ((req_storages[req_idxs]->has(e) && ...))
								{
									fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...);
								}
							}
						}
						, smallest->_occupied
					);
				}

			}

			template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_self_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t req_count = sizeof...(req_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);

				if constexpr (req_count && opt_count)
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };
					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					for (size_t i = 0; i < smallest->_occupied; i++)
					{
						entity_id e = smallest->_connector[i];
						if ((req_storages[req_idxs]->has(e) && ...))
						{
							fn(e, std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
						}
					}
				}

				else if constexpr (!req_count)
				{
					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					for (size_t i = 0; i < _entries_counter; i++)
					{
						entity_id e = _entity_entries[i];
						fn(e, std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
					}
				}
				else
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					for (size_t i = 0; i < smallest->_occupied; i++)
					{
						entity_id e = smallest->_connector[i];
						if ((req_storages[req_idxs]->has(e) && ...))
						{
							fn(e, std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...);
						}
					}
				}

			}

			template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_self_par_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t req_count = sizeof...(req_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);

				if constexpr (req_count && opt_count)
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};


					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };
					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->_connector[i];
								if ((req_storages[req_idxs]->has(e) && ...))
								{
									fn(e, std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
								}
							}
						}
						, smallest->_occupied
					);
				}

				else if constexpr (!req_count)
				{
					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = _entity_entries[i];
								fn(e, std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(e) : nullptr)...);
							}
						}
						, _entries_counter
					);

				}
				else
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};


					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					_query_par_engine.query
					(
						[&](size_t start, size_t end)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->_connector[i];
								if ((req_storages[req_idxs]->has(e) && ...))
								{
									fn(e, std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...);
								}
							}
						}
						, smallest->_occupied
					);
				}

			}


			template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
			inline void _query_with_impl(entity_id entity, Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
			{
				constexpr size_t req_count = sizeof...(req_idxs);
				constexpr size_t opt_count = sizeof...(opt_idxs);

				if constexpr (req_count && opt_count)
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };
					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					if (smallest->has(entity))
					{
						if ((req_storages[req_idxs]->has(entity) && ...))
						{
							fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(entity)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);
						}
					}
				}

				else if constexpr (!req_count)
				{
					storage_id opt_storage_keys[opt_count] =
					{
						get_storage_id<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>()...,
					};

					_internal::poly_storage* opt_storages[opt_count] = { (_storage_mask[opt_storage_keys[opt_idxs]] ? &_storage[opt_storage_keys[opt_idxs]] : nullptr)... };

					fn(std::forward<Params>(params)..., (opt_storages[opt_idxs] ? opt_storages[opt_idxs]->template get_if_has<std::remove_pointer_t<std::tuple_element_t<opt_idxs, opt_tuple>>>(entity) : nullptr)...);

				}
				else
				{
					storage_id require_storage_keys[req_count] =
					{
						get_storage_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
					};

					if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
					{
						return;
					}

					_internal::poly_storage* req_storages[req_count] = { &_storage[require_storage_keys[req_idxs]]... };

					_internal::poly_storage* smallest = req_storages[0];

					for (_internal::poly_storage* st : req_storages)
					{
						if (st->_occupied < smallest->_occupied)
						{
							smallest = st;
						}
					}

					if (smallest->has(entity))
					{
						if ((req_storages[req_idxs]->has(entity) && ...))
						{
							fn(std::forward<Params>(params)..., req_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(entity)...);
						}
					}
				}
			}

		private:
			inline bool _validate_entity(entity_id id) noexcept
			{
				if (id == nullent)
				{
					std::cout << "nullent entity usage" << '\n';
					return false;
				}
				else if (id >= _real_capacity)
				{
					std::cout << "invalid entity usage" << '\n';
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

			_internal::query_par_engine	_query_par_engine;

		private:
			static inline storage_id	_storage_id_counter	= 0;

		};
	}
}