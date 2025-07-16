#ifndef KAWA_ECS_REGISTRY
#define KAWA_ECS_REGISTRY

#include <array>

#include "internal/core.h"
#include "internal/meta.h"
#include "internal/meta_ecs_ext.h"
#include "internal/entity_manager.h"
#include "internal/poly_storage.h"
#include "internal/query_par_engine.h"

namespace kawa
{
namespace ecs
{
	typedef size_t entity_id;
	typedef size_t storage_id;

	struct registry_specification
	{
		size_t max_entity_count = 256;
		size_t max_component_types = 256;
		size_t thread_count = (std::thread::hardware_concurrency() / 2);
		std::string debug_name = "unnamed";
	};																										

	class registry
	{
	public:
		inline registry(const registry_specification& reg_spec) noexcept
			: _query_par_engine(reg_spec.thread_count)
			, _entity_manager(reg_spec.max_entity_count)
		{
			_spec = reg_spec;

			_storage			= new _internal::poly_storage[_spec.max_component_types];
			_storage_mask		= new bool[_spec.max_component_types]();
			_fetch_destroy_list = new size_t[_spec.max_entity_count];
		}

		inline ~registry() noexcept
		{
			delete[] _storage;
			delete[] _storage_mask;
			delete[] _fetch_destroy_list;
		}

		inline entity_id entity() noexcept
		{			
			return _entity_manager.get_new();
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
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			static_assert(!std::is_const_v<T>, "component can not have const qualifier");

			storage_id s_id = get_id<T>();

			bool& storage_cell = _storage_mask[s_id];

			_internal::poly_storage& storage = _storage[s_id];

			if (!storage_cell)
			{
				storage.template populate<T>(_spec.max_entity_count);
				storage_cell = true;
			}

			return storage.emplace<T>(entity, std::forward<Args>(args)...);
		}

		template<typename...Args>
		inline void erase(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
			{
				(([this, entity]<typename T>()
				{
					storage_id s_id = get_id<T>();
					_internal::poly_storage* storage = (_storage + s_id);

					if (_storage_mask[s_id])
					{
						storage->erase(entity);
					}
				}.template operator()<Args>()), ...);
			}
		}

		template<typename...Args>
		inline bool has(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
			{
				return (([this, entity] <typename T>()
				{
					storage_id s_id = get_id<T>();

					if (_storage_mask[s_id])
					{
						return _storage[s_id].has(entity);
					}

					return false;
				}. template operator()<Args>()) && ...);
			}

			return false;
		}

		template<typename T>
		inline T& get(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			storage_id s_id = get_id<T>();

			return _storage[s_id].get<T>(entity);
		}

		template<typename T>
		inline T* get_if_has(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
			{
				storage_id s_id = get_id<T>();

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
			KAWA_DEBUG_EXPAND(_validate_entity(from));
			KAWA_DEBUG_EXPAND(_validate_entity(to));

			if (from != to)
			{
				(([this, from, to]<typename T>()
				{
					storage_id s_id = get_id<T>();
					_storage[s_id].copy(from, to);

				}. template operator() < Args > ()), ...);
			}
		}

		template<typename...Args>
		inline void move(entity_id from, entity_id to) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(from));
			KAWA_DEBUG_EXPAND(_validate_entity(to));

			if (from != to)
			{
				(([this, from, to]<typename T>()
				{
					storage_id s_id = get_id<T>();

					_storage[s_id].move(from, to);

				}. template operator()<Args>()), ...);
			}
		}

		inline entity_id clone(entity_id from) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(from));

			entity_id e = entity();

			if (!e) return e;

			for (storage_id s_id = 0; s_id < _spec.max_component_types; s_id++)
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
			KAWA_DEBUG_EXPAND(_validate_entity(from));
			KAWA_DEBUG_EXPAND(_validate_entity(to));

			for (storage_id s_id = 0; s_id < _spec.max_component_types; s_id++)
			{
				if (_storage_mask[s_id])
				{
					_storage[s_id].copy(from, to);
				}
			}
		}

		inline void destroy(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
			{
				for (size_t s_id = 0; s_id < _spec.max_component_types; s_id++)
				{
					if (_storage_mask[s_id])
					{
						_storage[s_id].erase(entity);
					}
				}

				_entity_manager.remove_unchecked(entity);
			}
		}

		inline bool is_valid(entity_id e)
		{
			if (e == nullent || e >= _spec.max_entity_count)
			{
				return false;
			}

			return true;
		}

		inline void fetch_destroy(entity_id entity) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
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
				for (entity_id i : _entity_manager)
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
			KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_par inside another parallel query body", _spec.debug_name);

			_query_par_running = true;

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
					, _entity_manager.get_occupied()
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

			_query_par_running = false;

			_fetch_destroy_exec();
		}

		template<typename Fn, typename...Params>
		inline void query_self(Fn&& fn, Params&&...params) noexcept
		{
			using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

			static_assert(std::is_same_v<std::tuple_element_t<0, typename query_self_traits::args_tuple>, kawa::ecs::entity_id>, "query self fucntion must take kawa::ecs::entity_id as a first parameter");

			if constexpr (query_self_traits::args_count == query_self_traits::params_count)
			{
				for (entity_id i : _entity_manager)
				{
					fn(i, std::forward<Params>(params)...);
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
			KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_self_par inside another parallel query body", _spec.debug_name);

			_query_par_running = true;

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

							fn(_entity_manager.get_at_uncheked(i), std::forward<Params>(params)...);
						}
					}
					, _entity_manager.get_occupied()
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

			_query_par_running = false;

			_fetch_destroy_exec();
		}

		template<typename Fn, typename...Params>
		inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
		{
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			using query_traits = typename meta::query_traits<Fn, Params...>;

			if (_entity_manager.alive(entity))
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

				_entity_manager.remove_unchecked(e);
			}

			_fetch_destroy_list_size = 0;
		}

		template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t req_count = sizeof...(req_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);

			std::array<storage_id, req_count> require_storage_keys = 
			{
					get_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
			};

			if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
			{
				return;
			}

			std::array<storage_id, opt_count> optional_storage_keys =
			{
					get_id<std::tuple_element_t<opt_idxs, opt_tuple>>()...,
			};

			std::array<_internal::poly_storage*, req_count> require_storages = { &_storage[require_storage_keys[req_idxs]]... };
			std::array<_internal::poly_storage*, opt_count> optional_storages = { (_storage_mask[optional_storage_keys[opt_idxs]] ? &_storage[optional_storage_keys[opt_idxs]] : _storage[optional_storage_keys[opt_idxs]].template populate<std::tuple_element_t<opt_idxs, opt_tuple>>(_spec.max_entity_count))... };

			if constexpr (require_storages.size())
			{
				_internal::poly_storage* smallest = require_storages[0];

				for (_internal::poly_storage* st : require_storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				for (size_t i = 0; i < smallest->_occupied; i++)
				{
					entity_id e = smallest->_connector[i];
					if ((require_storages[req_idxs]->has(e) && ...))
					{
						fn
						(
							std::forward<Params>(params)...,
							require_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...,
							optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
						);
					}
				}
			}
				
			else
			{						
				for (entity_id e : _entity_manager)
				{
					fn
					(
						std::forward<Params>(params)..., 
						optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
					);
				}
			}
				
		}


		template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_par_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t req_count = sizeof...(req_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);

			std::array<storage_id, req_count> require_storage_keys =
			{
					get_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
			};

			if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
			{
				return;
			}

			std::array<storage_id, opt_count> optional_storage_keys =
			{
					get_id<std::tuple_element_t<opt_idxs, opt_tuple>>()...,
			};

			std::array<_internal::poly_storage*, req_count> require_storages = { &_storage[require_storage_keys[req_idxs]]... };
			std::array<_internal::poly_storage*, opt_count> optional_storages = { (_storage_mask[optional_storage_keys[opt_idxs]] ? &_storage[optional_storage_keys[opt_idxs]] : _storage[optional_storage_keys[opt_idxs]].template populate<std::tuple_element_t<opt_idxs, opt_tuple>>(_spec.max_entity_count))... };

			if constexpr (require_storages.size())
			{
				_internal::poly_storage* smallest = require_storages[0];

				for (_internal::poly_storage* st : require_storages)
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
							if ((require_storages[req_idxs]->has(e) && ...))
							{
								fn
								(
									std::forward<Params>(params)...,
									require_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...,
									optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
								);
							}
						}
					}
					, smallest->_occupied
				);
			}

			else
			{
				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						for (size_t i = start; i < end; ++i)
						{
							entity_id e = _entity_manager.get_at_uncheked(i);
							fn
							(
								std::forward<Params>(params)...,
								optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
							);
						}
					}
					, _entity_manager.get_occupied()
				);
			}
		}

		template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_self_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t req_count = sizeof...(req_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);

			std::array<storage_id, req_count> require_storage_keys =
			{
					get_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
			};

			if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
			{
				return;
			}

			std::array<storage_id, opt_count> optional_storage_keys =
			{
					get_id<std::tuple_element_t<opt_idxs, opt_tuple>>()...,
			};

			std::array<_internal::poly_storage*, req_count> require_storages = { &_storage[require_storage_keys[req_idxs]]... };
			std::array<_internal::poly_storage*, opt_count> optional_storages = { (_storage_mask[optional_storage_keys[opt_idxs]] ? &_storage[optional_storage_keys[opt_idxs]] : _storage[optional_storage_keys[opt_idxs]].template populate<std::tuple_element_t<opt_idxs, opt_tuple>>(_spec.max_entity_count))... };

			if constexpr (require_storages.size())
			{
				_internal::poly_storage* smallest = require_storages[0];

				for (_internal::poly_storage* st : require_storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				for (size_t i = 0; i < smallest->_occupied; i++)
				{
					entity_id e = smallest->_connector[i];
					fn
					(
						e,
						std::forward<Params>(params)...,
						require_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...,
						optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
					);
				}
			}

			else
			{
				for (entity_id e : _entity_manager)
				{
					fn
					(
						e,
						std::forward<Params>(params)...,
						optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
					);
				}
			}

		}

		template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_self_par_impl(Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t req_count = sizeof...(req_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);

			std::array<storage_id, req_count> require_storage_keys =
			{
					get_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
			};

			if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
			{
				return;
			}

			std::array<storage_id, opt_count> optional_storage_keys =
			{
					get_id<std::tuple_element_t<opt_idxs, opt_tuple>>()...,
			};

			std::array<_internal::poly_storage*, req_count> require_storages = { &_storage[require_storage_keys[req_idxs]]... };
			std::array<_internal::poly_storage*, opt_count> optional_storages = { (_storage_mask[optional_storage_keys[opt_idxs]] ? &_storage[optional_storage_keys[opt_idxs]] : _storage[optional_storage_keys[opt_idxs]].template populate<std::tuple_element_t<opt_idxs, opt_tuple>>(_spec.max_entity_count))... };

			if constexpr (require_storages.size())
			{
				_internal::poly_storage* smallest = require_storages[0];

				for (_internal::poly_storage* st : require_storages)
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
							if ((require_storages[req_idxs]->has(e) && ...))
							{
								fn
								(
									e,
									std::forward<Params>(params)...,
									require_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(e)...,
									optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
								);
							}
						}
					}
					, smallest->_occupied
				);
			}

			else
			{
				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						for (size_t i = start; i < end; ++i)
						{
							entity_id e = _entity_manager.get_at_uncheked(i);
							fn
							(
								e,
								std::forward<Params>(params)...,
								optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(e)...
							);
						}
					}
					, _entity_manager.get_occupied()
				);
			}
		}


		template<typename Fn, typename req_tuple, typename opt_tuple, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_with_impl(entity_id entity, Fn&& fn, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{

			constexpr size_t req_count = sizeof...(req_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);

			std::array<storage_id, req_count> require_storage_keys =
			{
					get_id<std::tuple_element_t<req_idxs, req_tuple>>()...,
			};

			if (!(_storage_mask[require_storage_keys[req_idxs]] && ...))
			{
				return;
			}

			std::array<storage_id, opt_count> optional_storage_keys =
			{
					get_id<std::tuple_element_t<opt_idxs, opt_tuple>>()...,
			};

			std::array<_internal::poly_storage*, req_count> require_storages = { &_storage[require_storage_keys[req_idxs]]... };
			std::array<_internal::poly_storage*, opt_count> optional_storages = { (_storage_mask[optional_storage_keys[opt_idxs]] ? &_storage[optional_storage_keys[opt_idxs]] : _storage[optional_storage_keys[opt_idxs]].template populate<std::tuple_element_t<opt_idxs, opt_tuple>>(_spec.max_entity_count))... };

			if constexpr (require_storages.size())
			{
				_internal::poly_storage* smallest = require_storages[0];

				for (_internal::poly_storage* st : require_storages)
				{
					if (st->_occupied < smallest->_occupied)
					{
						smallest = st;
					}
				}

				if ((require_storages[req_idxs]->has(entity) && ...))
				{
					fn
					(
						std::forward<Params>(params)...,
						require_storages[req_idxs]->template get<std::tuple_element_t<req_idxs, req_tuple>>(entity)...,
						optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(entity)...
					);
				}
			}
			else
			{
				fn
				(
					std::forward<Params>(params)...,
					optional_storages[opt_idxs]->template get_if_has<std::tuple_element_t<opt_idxs, opt_tuple>>(entity)...
				);
			}				
		}

	private:
		template<typename T>
		inline storage_id get_id() noexcept
		{
			static storage_id id = _storage_id_counter++;
			KAWA_DEBUG_EXPAND(_validate_storage(id));
			return id;
		}

		inline void _validate_entity(entity_id id) noexcept
		{
			KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", _spec.debug_name);
			KAWA_ASSERT_MSG(id < _spec.max_entity_count, "[ {} ]: invalid entity_id [ {} ] usage", _spec.debug_name, id);
		}

		inline void _validate_storage(storage_id id) noexcept
		{
			KAWA_ASSERT_MSG(id < _spec.max_component_types, "[ {} ]: max amoount of unique component types reached [ {} ], increase max_component_types", _spec.debug_name, _spec.max_component_types);
		}

	private:
		_internal::poly_storage*	_storage = nullptr;

		registry_specification		_spec;

		bool*						_storage_mask = nullptr;

		entity_manager				_entity_manager;

		size_t*						_fetch_destroy_list = nullptr;
		size_t						_fetch_destroy_list_size = 0;

		_internal::query_par_engine	_query_par_engine;
		bool						_query_par_running = false;

	private:
		static inline storage_id	_storage_id_counter = 0;
		

	};
}
}

#endif 