#ifndef KW_META_ECS_EXT
#define	KW_META_ECS_EXT

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

		template <typename T, template <typename> typename F>
		struct transform_tuple;

		template <template <typename> typename F, typename... Types>
		struct transform_tuple<std::tuple<Types...>, F>
		{
			using type = typename std::tuple<F<Types>...>;
		};

		template <typename T, template <typename...> typename F>
		using transform_tuple_t = typename transform_tuple<T, F>::type;

		template<typename Fn, typename...Params>
		struct query_traits
		{
			static constexpr size_t params_count = sizeof...(Params);

			using dirty_args_tuple = typename meta::function_traits<Fn>::args_tuple;
			using args_tuple = transform_tuple_t<dirty_args_tuple, std::remove_cvref_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = typename meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = typename meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = typename meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
		};

		template<typename Fn, typename...Params>
		struct query_self_traits
		{
			static constexpr size_t params_count = sizeof...(Params) + 1;

			using dirty_args_tuple = typename meta::function_traits<Fn>::args_tuple;
			using args_tuple = transform_tuple_t<dirty_args_tuple, std::remove_cvref_t>;

			static constexpr size_t args_count = std::tuple_size_v<args_tuple>;

			using no_params_args_tuple = meta::sub_tuple<params_count, std::tuple_size_v<args_tuple>>::template of<args_tuple>;

			static constexpr size_t opt_args_count = meta::get_ptr_type_count_tuple<no_params_args_tuple>();

			using require_args_tuple = meta::sub_tuple<0, std::tuple_size_v<no_params_args_tuple> -opt_args_count>::template of<no_params_args_tuple>;
			static constexpr size_t require_args_count = std::tuple_size_v<require_args_tuple>;

			using opt_args_tuple = meta::sub_tuple<std::tuple_size_v<no_params_args_tuple> -opt_args_count, std::tuple_size_v<no_params_args_tuple>>::template of<no_params_args_tuple>;
		};
	};
}
#endif