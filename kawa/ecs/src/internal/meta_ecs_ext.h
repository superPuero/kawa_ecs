#ifndef KAWA_META_ECS_EXT
#define	KAWA_META_ECS_EXT

#include "meta.h"
#include <type_traits>

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
		concept non_cv =	!std::is_const_v<std::remove_reference_t<T>> &&
							!std::is_volatile_v<std::remove_reference_t<T>>;
		
		template<typename Fn, size_t offset, typename...Params>
		consteval inline bool ensure_fallthrough_parameters_fn() noexcept
		{
			constexpr bool result = collect_pairs<std::is_convertible, std::tuple<Params...>, typename meta::query_traits<Fn, offset, Params...>::params_tuple>::val;

			static_assert(result, "All fall-through parameters must be convertable to corresponding query function parameters.");

			return result;
		}


		template<typename Fn, size_t index, typename T, typename...Params>
		consteval inline bool ensure_parameter_fn() noexcept
		{
			constexpr bool result = std::is_same_v<typename meta::function_traits<Fn>::template arg_at<sizeof...(Params) + index>, T>;

			static_assert(result, "Unexpected query parameter.");

			return result;
		}

		template<typename Fn, size_t offset, typename...Params>
		concept ensure_fallthrough_parameters = ensure_fallthrough_parameters_fn<Fn, offset, Params...>();
			

		template<typename Fn, size_t index, typename T, typename...Params>
		concept ensure_parameter = ensure_parameter_fn<Fn, index, T, Params...>();

	};
}
#endif