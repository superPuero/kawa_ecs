#ifndef KW_ECS_REGISTRY
#define KW_ECS_REGISTRY

#include "internal/core.h"
#include "internal/meta.h"
#include "internal/meta_ecs_ext.h"
#include "internal/poly_storage.h"
#include "internal/query_par_engine.h"

#define KW_MAX_UNIQUE_STORAGE_COUNT 255

#ifndef KAWA_ECS_PARALLELISM
	#define KAWA_ECS_PARALLELISM (std::thread::hardware_concurrency() / 2)
#endif

namespace kawa
{
	namespace ecs
	{
		typedef size_t entity_id;
		typedef size_t storage_id;

		constexpr inline entity_id nullent = 0;

		class registry
		{
			template<typename T>
			static inline storage_id get_storage_id() noexcept
			{
				static storage_id id = _storage_id_counter++;
				KW_ASSERT_MSG(id < KW_MAX_UNIQUE_STORAGE_COUNT, "max amoount os storage ids reached, increase KW_MAX_UNIQUE_STORAGE_COUNT for more avaliable storage ids");
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
				_fetch_destroy_list = new size_t[_real_capacity];
				_free_list = new size_t[_real_capacity]();
				_entity_mask = new bool[_real_capacity]();
				_entity_entries = new size_t[_real_capacity]();
				_r_entity_entries = new size_t[_real_capacity]();
			}

			inline ~registry() noexcept
			{
				delete[] _storage;
				delete[] _storage_mask;
				delete[] _entity_mask;
				delete[] _fetch_destroy_list;
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
				KW_ASSERT(_validate_entity(entity));

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
				KW_ASSERT(_validate_entity(entity));

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
				KW_ASSERT(_validate_entity(entity));

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
				KW_ASSERT(_validate_entity(entity));

				storage_id s_id = get_storage_id<T>();

				return _storage[s_id].get<T>(entity);
			}

			template<typename T>
			inline T* get_if_has(entity_id entity) noexcept
			{
				KW_ASSERT(_validate_entity(entity));

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
				KW_ASSERT(_validate_entity(from));
				KW_ASSERT(_validate_entity(to));

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
				KW_ASSERT(_validate_entity(from));
				KW_ASSERT(_validate_entity(to));

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
				KW_ASSERT(_validate_entity(from));

				entity_id e = entity();

				if (!e) return e;

				for (storage_id s_id = 0; s_id < KW_MAX_UNIQUE_STORAGE_COUNT; s_id++)
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
				KW_ASSERT(_validate_entity(from));
				KW_ASSERT(_validate_entity(to));

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
				KW_ASSERT(_validate_entity(entity));

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

			inline void fetch_destroy(entity_id entity) noexcept
			{
				KW_ASSERT(_validate_entity(entity));

				bool& entity_cell = _entity_mask[entity];

				if (entity_cell)
				{
					_fetch_destroy_list[_fetch_destroy_list_size++] = entity;
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
				_fetch_destroy_exec();	
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

				_fetch_destroy_exec();
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

				_fetch_destroy_exec();
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

				_fetch_destroy_exec();
			}

			template<typename Fn, typename...Params>
			inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
			{
				KW_ASSERT(_validate_entity(entity));

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

				_fetch_destroy_exec();
			}

		private:
			inline void _fetch_destroy_exec() noexcept
			{
				for (size_t i = 0; i < _fetch_destroy_list_size; i++)
				{
					entity_id e = _fetch_destroy_list[i];

					for (size_t s_id = 0; s_id < _storage_id_counter; s_id++)
					{
						if (_storage_mask[s_id])
						{
							_storage[s_id].erase(e);
						}
					}

					_free_list[_free_list_size++] = e;
					_entity_mask[e] = false;

					size_t idx = _r_entity_entries[e];

					_entity_entries[idx] = _entity_entries[--_entries_counter];
					_r_entity_entries[_entity_entries[_entries_counter]] = idx;
				}

				_fetch_destroy_list_size = 0;
			}

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
			_internal::poly_storage*	_storage = nullptr;

			size_t						_capacity = 0;
			size_t						_real_capacity = 0;
			size_t						_occupied = 1;

			bool*						_storage_mask = nullptr;

			bool*						_entity_mask = nullptr;

			size_t*						_entity_entries = nullptr;
			size_t*						_r_entity_entries = nullptr;
			size_t						_entries_counter = 0;

			size_t*						_fetch_destroy_list = nullptr;
			size_t						_fetch_destroy_list_size = 0;

			size_t*						_free_list = nullptr;
			size_t						_free_list_size = 0;

			_internal::query_par_engine	_query_par_engine;

		private:
			static inline storage_id	_storage_id_counter = 0;

		};
	}
}

#endif 