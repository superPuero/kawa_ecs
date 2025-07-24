#ifndef KAWA_META_ECS_EXT
#define	KAWA_META_ECS_EXT

#include <type_traits>

#include "meta.h"
#include "ecs_base.h"

namespace kawa
{
	namespace meta
	{			
		template<typename Fn, size_t offset, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + offset;


			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;

			using params_tuple = typename sub_tuple<dirty_args_tuple, offset, params_count>::tuple;

			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;
																											
			using no_params_args_tuple = sub_tuple<args_tuple, params_count, args_count>::tuple;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;

			using params_tuple = typename sub_tuple<dirty_args_tuple, 1, params_count>::tuple;

			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<args_tuple, params_count, args_count>::tuple;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};


		template<typename T>
		consteval inline bool valid_component_fn() noexcept
		{
			const bool result = !std::is_const_v<T> &&
								!std::is_volatile_v<T>;

			static_assert(!std::is_const_v<T>, "Component allowed to be const qualified only in query context.");

			static_assert(!std::is_volatile_v<T>, "Component allowed to be volatile qualified only in query context.");

			return result;
		}

		template<typename T>
		concept valid_component = valid_component_fn<T>();


		template<typename Fn, size_t offset, typename...Params>
		consteval inline bool ensure_fallthrough_parameters_fn() noexcept
		{
			constexpr bool result = collect_pairs<std::is_convertible, std::tuple<Params...>, typename meta::query_traits<Fn, offset, Params...>::params_tuple>::val;

			static_assert(result, "All fall-through parameters must be convertable to corresponding query function parameters.");

			return result;
		}

		template<typename Fn, size_t offset, typename...Params>
		concept ensure_fallthrough_parameters = ensure_fallthrough_parameters_fn<Fn, offset, Params...>();

		template<typename Fn, size_t index, typename T, typename...Params>
		consteval inline bool ensure_parameter_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, T>;

			static_assert(result, "Unexpected query parameter.");

			return result;
		}

		template<typename Fn, size_t index, typename T, typename...Params>
		concept ensure_parameter = ensure_parameter_fn<Fn, index, T, Params...>();

		template<typename Fn, size_t index, typename...Params>
		consteval inline bool ensure_entity_id_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, kawa::ecs::entity_id>;

			static_assert(result, "Unexpected query function parameter, expected kawa::ecs::entity_id.");

			return result;
		}

		template<typename Fn, size_t index, typename...Params>
		concept ensure_entity_id = ensure_entity_id_fn<Fn, index, Params...>();

		template<typename Fn, size_t index, typename...Params>
		consteval inline bool ensure_component_info_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, kawa::ecs::component_info>;

			static_assert(result, "Unexpected query function parameter, expected kawa::ecs::component_info.");

			return result;
		}

		template<typename Fn, size_t index, typename...Params>
		concept ensure_component_info = ensure_component_info_fn<Fn, index, Params...>();

	};
}
#endif