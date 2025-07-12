#ifndef KW_META_ECS_EXT
#define	KW_META_ECS_EXT

#include "meta.h"

namespace kawa
{
	namespace meta
	{
		template<typename...Types>
		constexpr size_t get_ptr_type_count_impl()
		{
			return (0 + ... + (std::is_pointer_v<Types> ? 1 : 0));
		}

		template<typename...Types>
		struct get_ptr_type_count
		{
			constexpr static size_t value = get_ptr_type_count_impl<Types...>();
		};

		template<typename...Types>
		struct get_ptr_type_count<std::tuple<Types...>>
		{
			constexpr static size_t value = get_ptr_type_count_impl<Types...>();
		};

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

		template <typename T, template <typename> typename F>
		struct transform_each;

		template <typename...Types, template <typename> typename F>
		struct transform_each<std::tuple<Types...>, F>
		{
			using type = typename std::tuple<F<Types>...>;
		};

		template <typename T, template <typename...> typename F>
		using transform_each_t = typename transform_each<T, F>::type;

		template<typename Fn, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params);

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, std::remove_cvref_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = typename sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = get_ptr_type_count<no_params_args_tuple>::value;

			using require_args_tuple = typename sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = typename sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename function_traits<Fn>::args_tuple;
			using args_tuple = transform_each_t<dirty_args_tuple, std::remove_cvref_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = get_ptr_type_count<no_params_args_tuple>::value;

			using require_args_tuple = sub_tuple<0, std::tuple_size_v<no_params_args_tuple> - opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
		};
	};
}
#endif