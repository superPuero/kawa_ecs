#ifndef KAWA_ECS_REGISTRY
#define KAWA_ECS_REGISTRY

#include <array>
#include <concepts>

#include "internal/core.h"
#include "internal/meta.h"
#include "internal/meta_ecs_ext.h"
#include "internal/poly_storage.h"
#include "internal/entity_manager.h"
#include "internal/storage_manager.h"
#include "internal/thread_pool.h"

namespace kawa
{
namespace ecs
{
class registry
{
public:
	struct specification
	{
		std::string name = "unnamed";
		size_t max_entity_count = 512;
		size_t max_component_types = 32;
	};

	inline registry(const registry::specification& reg_spec) noexcept
		: _spec(reg_spec)
		, _storage_manager(reg_spec.max_component_types, reg_spec.max_entity_count, reg_spec.name)
		, _entity_manager(reg_spec.max_entity_count, reg_spec.name)
	{}

	inline registry(const registry& other) noexcept = default;
	inline registry(registry&& other) noexcept = default;

	inline registry& operator=(const registry& reg) noexcept = default;
	inline registry& operator=(registry&& reg) noexcept = default;

	inline ~registry() noexcept {};

public:
	inline const specification& get_specs() const noexcept
	{
		return _spec;
	}

	inline std::string get_full_name() const noexcept
	{
		if (_owned_by_world)
		{
			return std::format("{}::{}", _world_name, _spec.name);
		}

		return _spec.name;

	}

	inline void clear() noexcept
	{
		_entity_manager.clear();
		_storage_manager.clear();
	}

	inline entity_id entity() noexcept
	{			
		return _entity_manager.get_new();
	}

	template<meta::valid_component...Args>
	inline entity_id entity_with(Args&&...args) noexcept
	{
		entity_id id = entity();

		(emplace<Args>(id, std::forward<Args>(args)), ...);

		return id;
	}

	template<meta::valid_component T, typename...Args>
		requires std::constructible_from<T, Args...> 
	inline T& emplace(entity_id entity, Args&&...args) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));
		KAWA_ASSERT_MSG(_entity_manager.alive(entity), "[ {} ] kawa::ecs::registry::emplace<{}> on non alive entity", get_full_name(), meta::type_name<T>());

		static_assert(!std::is_const_v<T>, "component can not have const qualifier");	   

		return _storage_manager.emplace<T>(entity, std::forward<Args>(args)...);
	}

	template<meta::valid_component...Args>
	inline void erase(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		_storage_manager.erase<Args...>(entity);
	}

	template<meta::valid_component...Args>
	inline bool has(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		return _storage_manager.has<Args...>(entity);
	}

	template<meta::valid_component T>
	inline T& get(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		return _storage_manager.get<T>(entity);
	}

	template<meta::valid_component T>
	inline T* get_if_has(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		if (!_entity_manager.alive(entity))
		{
			return nullptr;
		}

		return _storage_manager.get_if_has<T>(entity);
	}

	template<meta::valid_component...Args>
		requires ((std::copyable<Args> && ...))
	inline void copy(entity_id from, entity_id to) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(from));
		KAWA_DEBUG_EXPAND(_validate_entity(to));

		_storage_manager.copy<Args...>(from, to);
	}

	template<meta::valid_component...Args>
		requires ((std::movable<Args> && ...))
	inline void move(entity_id from, entity_id to) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(from));
		KAWA_DEBUG_EXPAND(_validate_entity(to));

		_storage_manager.move<Args...>(from, to);
	}

	inline entity_id clone(entity_id from) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(from));

		entity_id e = entity();

		if (e == nullent) return e;

		for (storage_id s : _storage_manager)
		{
			_storage_manager.get_storage(s).copy(from, e);
		}

		return e;
	}

	inline void clone(entity_id from, entity_id to) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(from));
		KAWA_DEBUG_EXPAND(_validate_entity(to));

		for (storage_id s : _storage_manager)
		{
			_storage_manager.get_storage(s).copy(from, to);
		}
	}

	inline void destroy(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		if (_entity_manager.alive(entity))
		{
			for (storage_id s : _storage_manager)
			{
				_storage_manager.get_storage(s).erase(entity);
			}

			_entity_manager.remove_unchecked(entity);
		}
	}

	inline bool alive(entity_id entity) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		return _storage_manager.alive(entity);
	}

	inline bool is_valid(entity_id e) noexcept
	{									
		if (e == nullent || e >= _spec.max_entity_count)
		{
			return false;
		}

		return true;
	}
		   
	template<typename Fn>
		requires meta::ensure_entity_id<Fn, 0>
	inline void on_construct(Fn&& fn)
	{
		using ft = typename meta::function_traits<Fn>;

		using second_arg = typename ft::template arg_at<1>;

		_storage_manager.get_storage<std::remove_cvref_t<second_arg>>().set_on_construct(std::forward<Fn>(fn));
	}

	template<typename Fn>
		requires meta::ensure_entity_id<Fn, 0>
	inline void on_destroy(Fn&& fn)
	{
		using ft = typename meta::function_traits<Fn>;

		using second_arg = typename ft::template arg_at<1>;

		_storage_manager.get_storage<std::remove_cvref_t<second_arg>>().set_on_destroy(std::forward<Fn>(fn));
	}

	template<meta::valid_component...Args>
	inline void ensure() noexcept
	{
		_storage_manager.ensure<Args...>();
	}

	template<typename Fn, typename...Params>
		requires 
		(
			meta::ensure_component_info<Fn, 0>&&
			meta::ensure_fallthrough_parameters<Fn, 1, Params...>
		)
	inline void query_with_info(entity_id entity, Fn&& fn, Params&&...params) noexcept
	{	
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		if (_entity_manager.alive(entity))
		{
			for (storage_id s : _storage_manager)
			{
				auto& storage = _storage_manager.get_storage(s);
				if (storage.has(entity))
				{
					std::forward<Fn>(fn)(std::forward<Params>(params)..., storage.get_type_info());
				}
			}
		}
	}

	template<typename Fn, typename...Params>
		requires 
		(	
			meta::ensure_entity_id<Fn, 0> && 
			meta::ensure_component_info<Fn, 1> &&
			meta::ensure_fallthrough_parameters<Fn, 2, Params...>
		)
	inline void query_self_info(Fn&& fn, Params&&...params) noexcept
	{
		for(entity_id entity : _entity_manager)
		{
			for (storage_id s : _storage_manager)
			{

				auto& storage = _storage_manager.get_storage(s);

				if (storage.has(entity))
				{
					std::forward<Fn>(fn)(entity, std::forward<Params>(params)..., storage.get_type_info());
				}
			}
		}
	}

	template<typename Fn, typename...Params>
		requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
	inline void query(Fn&& fn, Params&&...params) noexcept
	{
		using query_traits = typename meta::query_traits<Fn, 0, Params...>;

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
	}

	template<typename Fn, typename...Params>
		requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
	inline void query_par(thread_pool& exec, Fn&& fn, Params&&...params) noexcept
	{
		KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_par inside another parallel query body", get_full_name());

		_query_par_running = true;

		using query_traits = typename meta::query_traits<Fn, 0, Params...>;

		if constexpr (query_traits::args_count == query_traits::params_count)
		{
			exec.task
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
					exec,
					std::forward<Fn>(fn),
					std::make_index_sequence<query_traits::no_params_args_count>{},
					std::make_index_sequence<query_traits::require_count>{},
					std::make_index_sequence<query_traits::optional_count>{},
					std::forward<Params>(params)...
				);
		}

		_query_par_running = false;
	}

	template<typename Fn, typename...Params>
		requires
		(
			meta::ensure_entity_id<Fn, 0>&&
			meta::ensure_fallthrough_parameters<Fn, 1, Params...>
		)
	inline void query_self(Fn&& fn, Params&&...params) noexcept
	{
		using query_self_traits = typename meta::query_self_traits<Fn, Params...>;

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
	}

	template<typename Fn, typename...Params>
		requires
		(
			meta::ensure_entity_id<Fn, 0>&&
			meta::ensure_fallthrough_parameters<Fn, 1, Params...>
		)
	inline void query_self_par(thread_pool& exec, Fn&& fn, Params&&...params) noexcept
	{
		KAWA_ASSERT_MSG(!_query_par_running, "[ {} ]: trying to invoke kawa::ecs::query_self_par inside another parallel query body", get_full_name());
		_query_par_running = true;

		using query_traits = typename meta::query_traits<Fn, 1, Params...>;

		if constexpr (query_traits::args_count == query_traits::params_count)
		{
			exec.task
			(
				[&](size_t start, size_t end)
				{
					for (size_t i = start; i < end; i++)
					{
						fn(_entity_manager[i], std::forward<Params>(params)...);
					}
				}
				, _entity_manager.occupied()
			);
		}
		else
		{
			_query_self_par_impl<Fn, typename query_traits::no_params_args_tuple>
				(
					exec,
					std::forward<Fn>(fn),
					std::make_index_sequence<query_traits::no_params_args_count>{},
					std::make_index_sequence<query_traits::require_count>{},
					std::make_index_sequence<query_traits::optional_count>{},
					std::forward<Params>(params)...
				);
		}

		_query_par_running = false;
	}

	template<typename Fn, typename...Params>			
		requires meta::ensure_fallthrough_parameters<Fn, 0, Params...>
	inline void query_with(entity_id entity, Fn fn, Params&&...params) noexcept
	{
		KAWA_DEBUG_EXPAND(_validate_entity(entity));

		using query_traits = typename meta::query_traits<Fn, 0, Params...>;

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
	}

private:
	template<typename Fn, typename args_tuple, size_t...args_idxs, size_t...req_idxs, size_t...opt_idxs, typename...Params>
	inline void _query_impl(Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
	{
		constexpr size_t args_count = sizeof...(args_idxs);
		constexpr size_t opt_count = sizeof...(opt_idxs);
		constexpr size_t req_count = sizeof...(req_idxs);

		auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

		std::array<_::poly_storage*, opt_count> optional_storages;
		_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

		if constexpr (req_count)
		{
			std::array<_::poly_storage*, req_count> require_storages = {};
			_populate_required_array<args_tuple, args_idxs...>(require_storages);

			_::poly_storage* smallest = require_storages[0];

			for (_::poly_storage* st : require_storages)
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
	inline void _query_par_impl(thread_pool& exec, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
	{
		constexpr size_t args_count = sizeof...(args_idxs);
		constexpr size_t opt_count = sizeof...(opt_idxs);
		constexpr size_t req_count = sizeof...(req_idxs);

		auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

		std::array<_::poly_storage*, opt_count> optional_storages;
		_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

		if constexpr (req_count)
		{
			std::array<_::poly_storage*, req_count> require_storages = {};
			_populate_required_array<args_tuple, args_idxs...>(require_storages);

			_::poly_storage* smallest = require_storages[0];

			for (_::poly_storage* st : require_storages)
			{
				if (st->occupied() < smallest->occupied())
				{
					smallest = st;
				}
			}

			exec.task
			(
				[&](size_t start, size_t end)
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->get(i);
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
			exec.task
			(
				[&](size_t start, size_t end)
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = _entity_manager[i];
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

		auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

		std::array<_::poly_storage*, opt_count> optional_storages;
		_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

		if constexpr (req_count)
		{
			std::array<_::poly_storage*, req_count> require_storages = {};
			_populate_required_array<args_tuple, args_idxs...>(require_storages);

			_::poly_storage* smallest = require_storages[0];

			for (_::poly_storage* st : require_storages)
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
	inline void _query_self_par_impl(thread_pool& exec, Fn&& fn, std::index_sequence<args_idxs...>, std::index_sequence<req_idxs...>, std::index_sequence<opt_idxs...>, Params&&...params) noexcept
	{
		constexpr size_t args_count = sizeof...(args_idxs);
		constexpr size_t opt_count = sizeof...(opt_idxs);
		constexpr size_t req_count = sizeof...(req_idxs);

		auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

		std::array<_::poly_storage*, opt_count> optional_storages;
		_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

		if constexpr (req_count)
		{
			std::array<_::poly_storage*, req_count> require_storages = {};
			_populate_required_array<args_tuple, args_idxs...>(require_storages);

			_::poly_storage* smallest = require_storages[0];

			for (_::poly_storage* st : require_storages)
			{
				if (st->occupied() < smallest->occupied())
				{
					smallest = st;
				}
			}

			exec.task
			(
				[&](size_t start, size_t end)
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = smallest->get(i);
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
			exec.task
			(
				[&](size_t start, size_t end)
				{
					std::apply
					(
						[&](auto&&...storages)
						{
							for (size_t i = start; i < end; ++i)
							{
								entity_id e = _entity_manager[i];
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

		auto args_storages = std::forward_as_tuple(_storage_manager.get_storage<std::remove_pointer_t<std::remove_reference_t<std::tuple_element_t<args_idxs, args_tuple>>>>()...);

		std::array<_::poly_storage*, opt_count> optional_storages;
		_populate_optional_array<args_tuple, args_idxs...>(optional_storages);

		if constexpr (req_count)
		{
			std::array<_::poly_storage*, req_count> require_storages = {};
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
	inline void _make_owned(const std::string& world_name) noexcept
	{
		_owned_by_world = true;
		_world_name = world_name;
	}


private:
	template<typename args_tuple, size_t...I, size_t N>
	inline constexpr void _populate_optional_array(std::array<_::poly_storage*, N>& out) noexcept
	{
		size_t id = 0;

		(([&]<size_t i>()
		{
			using T = std::tuple_element_t<I, args_tuple>;

			if constexpr (std::is_pointer_v<T>)
			{
				using CleanT = std::remove_pointer_t<T>;
				auto key = get_id<CleanT>();
				out[id] = &_storage_manager.get_storage(key);
				id++;
			}
		}.template operator()<I>(), void(), 0), ...);
	}

	template<typename args_tuple, size_t...I, size_t N>
	inline constexpr void _populate_required_array(std::array<_::poly_storage*, N>& out) noexcept
	{
		size_t id = 0;

		(([&]<size_t i>()
		{
			using T = std::tuple_element_t<I, args_tuple>;

			if constexpr (std::is_reference_v<T>)
			{
				using CleanT = std::remove_reference_t<T>;
				auto key = get_id<CleanT>();
				out[id] = &_storage_manager.get_storage(key);
				id++;
			}
		}.template operator()<I>(), void(), 0), ...);
	}

private:
	template<typename T>
	inline storage_id get_id() noexcept
	{
		static storage_id id = _storage_manager.get_id<T>();
		KAWA_DEBUG_EXPAND(_validate_storage(id));
		return id;
	}

	inline void _validate_entity(entity_id id) const noexcept
	{
		KAWA_ASSERT_MSG(id != nullent, "[ {} ]: nullent usage", get_full_name());
		KAWA_ASSERT_MSG(id < _spec.max_entity_count, "[ {} ]: invalid entity_id [ {} ] usage", get_full_name(), id);
	}

	inline void _validate_storage(storage_id id) const noexcept
	{
		KAWA_ASSERT_MSG(id < _spec.max_component_types, "[ {} ]: maximum amoount of unique component types reached [ {} ], increase max_component_types", get_full_name(), _spec.max_component_types);
	}

private:
	specification				_spec;

	_::storage_manager			_storage_manager;
	_::entity_manager			_entity_manager;
	std::string					_world_name;

	bool						_query_par_running = false;
	bool						_owned_by_world = false;

private:
	static inline storage_id	_storage_id_counter = 0;		

};
}
}

#endif 