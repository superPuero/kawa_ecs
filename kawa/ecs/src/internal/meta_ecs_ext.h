#ifndef KAWA_META_ECS_EXT
#define	KAWA_META_ECS_EXT

#include "meta.h"
#include <type_traits>

namespace kawa
{
	namespace meta
	{			
		template<typename Fn, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params);

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;
																											
			using no_params_args_tuple = sub_tuple<params_count, args_count>::template of<args_tuple>;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, remove_suffix_cv_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<params_count, args_count>::template of<args_tuple>;
			static constexpr size_t no_params_args_count = std::tuple_size_v<no_params_args_tuple>;

			constexpr static size_t require_count = meta::lval_ref_type_count<no_params_args_tuple>::value;
			constexpr static size_t optional_count = meta::ptr_type_count<no_params_args_tuple>::value;
		};
	};
}
#endif