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

	typedef meta::type_info component_info;

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

			_storage			= new poly_storage[_spec.max_component_types];
			_storage_mask		= new bool[_spec.max_component_types]();
			_fetch_destroy_list = new entity_id[_spec.max_entity_count];
		}

		inline ~registry() noexcept
		{
			delete[] _storage;
			delete[] _storage_mask;
			delete[] _fetch_destroy_list;
		}

	public:
		inline const registry_specification& get_specs() const noexcept
		{
			return _spec;
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

			poly_storage& storage = _storage[s_id];

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
					poly_storage* storage = (_storage + s_id);

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

		inline bool is_valid(entity_id e) noexcept
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
		inline void query_with_info(entity_id entity, Fn&& fn, Params&&...params) noexcept
		{	
			KAWA_DEBUG_EXPAND(_validate_entity(entity));

			if (_entity_manager.alive(entity))
			{
				for (storage_id s = 0; s < _storage_id_counter; s++)
				{
					if (_storage_mask[s])
					{ 
						auto& storage = _storage[s];
						if (storage.has(entity))
						{
							fn(std::forward<Params>(params)..., storage.get_type_info());
						}
					}
				}
			}
		}

		template<typename Fn, typename...Params>
		inline void query_self_info(Fn&& fn, Params&&...params) noexcept
		{
			for(entity_id entity : _entity_manager)
			{
				for (storage_id s = 0; s < _storage_id_counter; s++)
				{
					if (_storage_mask[s])
					{
						auto& storage = _storage[s];
						if (storage.has(entity))
						{
							fn(entity, std::forward<Params>(params)..., storage.get_type_info());
						}
					}
				}
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
				_query_impl<Fn, typename query_traits::no_params_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<query_traits::no_params_args_count>{},
						std::make_index_sequence<query_traits::require_count>{},
						std::make_index_sequence<query_traits::optional_count>{},
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
					, _entity_manager.occupied()
				);
			}
			else
			{
				_query_par_impl<Fn, typename query_traits::no_params_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<query_traits::no_params_args_count>{},
						std::make_index_sequence<query_traits::require_count>{},
						std::make_index_sequence<query_traits::optional_count>{},
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
				_query_self_impl<Fn, typename query_self_traits::no_params_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<query_self_traits::no_params_args_count>{},
						std::make_index_sequence<query_self_traits::require_count>{},
						std::make_index_sequence<query_self_traits::optional_count>{},
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
					, _entity_manager.occupied()
				);
			}
			else
			{
				_query_self_par_impl<Fn, typename query_self_traits::no_params_args_tuple>
					(
						std::forward<Fn>(fn),
						std::make_index_sequence<query_self_traits::no_params_args_count>{},
						std::make_index_sequence<query_self_traits::require_count>{},
						std::make_index_sequence<query_self_traits::optional_count>{},
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
					_query_with_impl<Fn, typename query_traits::no_params_args_tuple>
						(
							entity,
							std::forward<Fn>(fn),
							std::make_index_sequence<query_traits::no_params_args_count>{},
							std::make_index_sequence<query_traits::require_count>{},
							std::make_index_sequence<query_traits::optional_count>{},
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

		template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t args_count = sizeof...(args_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);
			constexpr size_t req_count = sizeof...(req_idxs);

			auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

			std::array<poly_storage*, opt_count> optional_storages;
			_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

			if constexpr (req_count)
			{
				std::array<poly_storage*, req_count> require_storages = {};
				_populate_required_array<args_tuple, args_idxs...>(require_storages);

				poly_storage* smallest = require_storages[0];

				for (poly_storage* st : require_storages)
				{
					if (st->occupied() < smallest->occupied())
					{
						smallest = st;
					}
				}

				std::apply
				(
					[&](auto&&...storages)
					{
						for (entity_id e : *smallest)
						{
							if ((require_storages[req_idxs]->has(e) && ...))
							{
								fn
								(
									std::forward<Params>(params)...,
									storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
								);
							}
						}
					},
					args_storages
				);
			}
			else
			{
				std::apply
				(
					[&](auto&&...storages)
					{
						for (entity_id e : _entity_manager)
						{
							fn
							(
								std::forward<Params>(params)...,
								storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
							);
						}
					},
					args_storages
				);
			}
		}

		template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_par_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t args_count = sizeof...(args_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);
			constexpr size_t req_count = sizeof...(req_idxs);

			auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

			std::array<poly_storage*, opt_count> optional_storages;
			_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

			if constexpr (req_count)
			{
				std::array<poly_storage*, req_count> require_storages = {};
				_populate_required_array<args_tuple, args_idxs...>(require_storages);

				poly_storage* smallest = require_storages[0];

				for (poly_storage* st : require_storages)
				{
					if (st->occupied() < smallest->occupied())
					{
						smallest = st;
					}
				}

				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						std::apply
						(
							[&](auto&&...storages)
							{
								for (size_t i = start; i < end; ++i)
								{
									entity_id e = smallest->get_at(i);
									if ((require_storages[req_idxs]->has(e) && ...))
									{
										fn
										(
											std::forward<Params>(params)...,
											storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
										);
									}
								}
							},
							args_storages
						);
					}
					, smallest->occupied()
				);
			}
			else
			{
				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						std::apply
						(
							[&](auto&&...storages)
							{
								for (size_t i = start; i < end; ++i)
								{
									entity_id e = _entity_manager.get_at_uncheked(i);
									fn
									(
										std::forward<Params>(params)...,
										storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
									);
								}
							},
							args_storages
						);
					}
					, _entity_manager.occupied()
				);
			}
		}

		template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_self_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t args_count = sizeof...(args_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);
			constexpr size_t req_count = sizeof...(req_idxs);

			auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

			std::array<poly_storage*, opt_count> optional_storages;
			_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

			if constexpr (req_count)
			{
				std::array<poly_storage*, req_count> require_storages = {};
				_populate_required_array<args_tuple, args_idxs...>(require_storages);

				poly_storage* smallest = require_storages[0];

				for (poly_storage* st : require_storages)
				{
					if (st->occupied() < smallest->occupied())
					{
						smallest = st;
					}
				}

				std::apply
				(
					[&](auto&&...storages)
					{
						for (entity_id e : *smallest)
						{
							if ((require_storages[req_idxs]->has(e) && ...))
							{
								fn
								(
									e,
									std::forward<Params>(params)...,
									storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
								);
							}
						}
					},
					args_storages
				);
			}
			else
			{
				std::apply
				(
					[&](auto&&...storages)
					{
						for (entity_id e : _entity_manager)
						{
							fn
							(
								e,
								std::forward<Params>(params)...,
								storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
							);
						}
					},
					args_storages
				);
			}
		}

		template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_self_par_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t args_count = sizeof...(args_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);
			constexpr size_t req_count = sizeof...(req_idxs);

			auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

			std::array<poly_storage*, opt_count> optional_storages;
			_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

			if constexpr (req_count)
			{
				std::array<poly_storage*, req_count> require_storages = {};
				_populate_required_array<args_tuple, args_idxs...>(require_storages);

				poly_storage* smallest = require_storages[0];

				for (poly_storage* st : require_storages)
				{
					if (st->occupied() < smallest->occupied())
					{
						smallest = st;
					}
				}

				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						std::apply
						(
							[&](auto&&...storages)
							{
								for (size_t i = start; i < end; ++i)
								{
									entity_id e = smallest->get_at(i);
									if ((require_storages[req_idxs]->has(e) && ...))
									{
										fn
										(
											e,
											std::forward<Params>(params)...,
											storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
										);
									}
								}
							},
							args_storages
						);
					}
					, smallest->occupied()
				);
			}
			else
			{
				_query_par_engine.query
				(
					[&](size_t start, size_t end)
					{
						std::apply
						(
							[&](auto&&...storages)
							{
								for (size_t i = start; i < end; ++i)
								{
									entity_id e = _entity_manager.get_at_uncheked(i);
									fn														
									(
										e,
										std::forward<Params>(params)...,
										storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(e)...
									);
								}
							},
							args_storages
						);
					}
					, _entity_manager.occupied()
				);
			}
		}


		template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
		inline void _query_with_impl(entity_id entity, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
		{
			constexpr size_t args_count = sizeof...(args_idxs);
			constexpr size_t opt_count = sizeof...(opt_idxs);
			constexpr size_t req_count = sizeof...(req_idxs);

			auto args_storages = std::forward_as_tuple(_storage[get_id<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()]...);

			std::array<poly_storage*, opt_count> optional_storages;
			_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

			if constexpr (req_count)
			{
				std::array<poly_storage*, req_count> require_storages = {};
				_populate_required_array<args_tuple, args_idxs...>(require_storages);

				std::apply
				(
					[&](auto&&...storages)
					{
						if ((require_storages[req_idxs]->has(entity) && ...))
						{
							fn
							(
								std::forward<Params>(params)...,
								storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(entity)...
							);
						}
					},
					args_storages
				);
			}
			else
			{
				std::apply
				(
					[&](auto&&...storages)
					{
						fn
						(
							std::forward<Params>(params)...,
							storages.template get_defer<std::tuple_element_t<args_idxs, args_tuple>>(entity)...
						);
					},
					args_storages
				);
			}
		}

	private:
		template<typename args_tuple, size_t...I, size_t N>
		constexpr void _populate_optional_array(std::array<poly_storage*, N>& out) noexcept
		{
			size_t id = 0;

			(([&]<size_t i>()
			{
				using T = std::tuple_element_t<I, args_tuple>;

				if constexpr (std::is_pointer_v<T>)
				{
					using CleanT = std::remove_pointer_t<T>;

					auto key = get_id<CleanT>();
					bool& cell = _storage_mask[key];
					if (cell)
					{
						out[id] = _storage + key;

						id++;
					}
					else
					{
						cell = true;
						out[id] = _storage[key].template populate<CleanT>(_spec.max_entity_count);
						id++;
					}
				}
			}.template operator() < I > (), void(), 0), ...);
		}

		template<typename args_tuple, size_t...I, size_t N>
		constexpr void _populate_required_array(std::array<poly_storage*, N>& out) noexcept
		{
			size_t id = 0;

			(([&]<size_t i>()
			{
				using T = std::tuple_element_t<I, args_tuple>;

				if constexpr (std::is_reference_v<T>)
				{
					using CleanT = std::remove_reference_t<T>;

					auto key = get_id<CleanT>();
					bool& cell = _storage_mask[key];
					if (cell)
					{
						out[id] = _storage + key;

						id++;
					}
					else
					{
						cell = true;
						out[id] = _storage[key].template populate<CleanT>(_spec.max_entity_count);
						id++;
					}
				}
			}.template operator()<I>(), void(), 0), ...);
		}

	private:
		template<typename T>
		inline storage_id get_id() noexcept
		{
			static storage_id id = _storage_id_counter++;
			KAWA_DEBUG_EXPAND(_validate_storage(id));
			return id;
		}

		inline void _validate_entity(entity_id id) const noexcept
		{
			KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", _spec.debug_name);
			KAWA_ASSERT_MSG(id < _spec.max_entity_count, "[ {} ]: invalid entity_id [ {} ] usage", _spec.debug_name, id);
		}

		inline void _validate_storage(storage_id id) const noexcept
		{
			KAWA_ASSERT_MSG(id < _spec.max_component_types, "[ {} ]: max amoount of unique component types reached [ {} ], increase max_component_types", _spec.debug_name, _spec.max_component_types);
		}

	private:
		registry_specification		_spec;
		poly_storage*				_storage = nullptr;
		bool*						_storage_mask = nullptr;

		storage_id*					_registered_ids = nullptr;
		const char** 				_registered_names = nullptr;

		entity_manager				_entity_manager;

		entity_id*					_fetch_destroy_list = nullptr;
		size_t						_fetch_destroy_list_size = 0;

		_internal::query_par_engine	_query_par_engine;
		bool						_query_par_running = false;

	private:
		static inline storage_id	_storage_id_counter = 0;
		

	};
}
}

#endif 