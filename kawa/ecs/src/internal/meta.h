#ifndef KAWA_META
#define KAWA_META

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <tuple>
#include <type_traits>

#if defined(__clang__)
#	define KAWA_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name() [T = "
#	define KAWA_META_TYPE_NAME_POSTFIX "]"
#elif defined(_MSC_VER)
#	define KAWA_META_PRETTYFUNC __FUNCSIG__	
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name<"
#	define KAWA_META_TYPE_NAME_POSTFIX ">(void) noexcept"
#elif defined(__GNUC__) 
#	define KAWA_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KAWA_META_TYPE_NAME_PREFIX "kawa::meta::type_name() [with T = "
#	define KAWA_META_TYPE_NAME_POSTFIX "; std::string_view = std::basic_string_view<char>]"
#else
#	error "Function signature macro not defined for this compiler."
#endif

namespace kawa
{
	namespace meta
	{
		namespace _internal
		{

			template<typename T>
			extern const T fake_object;

			inline constexpr std::string_view type_name_helper(std::string_view decorated_name)
			{
				size_t start = decorated_name.find(KAWA_META_TYPE_NAME_PREFIX);
				size_t end = decorated_name.find(KAWA_META_TYPE_NAME_POSTFIX);

#ifdef _MSC_VER	

				auto pre_name = decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
				size_t pre_name_space = pre_name.find_first_of(' ');

				if (pre_name_space != std::string_view::npos)
				{
					return pre_name.substr(pre_name_space + 1, pre_name.size() - pre_name_space);
				}
				return pre_name;
#else
				return decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
#endif 

			}

			constexpr uint64_t fnv1a_hash(std::string_view str) noexcept
			{
				constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;
				constexpr uint64_t fnv_prime = 1099511628211ull;

				uint64_t hash = fnv_offset_basis;

				for (char c : str)
				{
					hash ^= static_cast<uint64_t>(c);
					hash *= fnv_prime;
				}

				return hash;
			}
		}

		template<typename T>
		inline constexpr std::string_view type_name() noexcept
		{
			return _internal::type_name_helper(KAWA_META_PRETTYFUNC);
		}

		constexpr uint64_t string_hash(std::string_view str) noexcept
		{
			return _internal::fnv1a_hash(str);
		}

		template<typename T>
		inline constexpr uint64_t type_hash() noexcept
		{
			return string_hash(type_name<T>());
		}

		struct empty_type_info_t {};

		struct type_info
		{
			constexpr type_info()
				: name(type_name<empty_type_info_t>())
				, hash(type_hash<empty_type_info_t>()) {}

			constexpr type_info(std::string_view n, uint64_t h) noexcept
				: name(n)
				, hash(h) {}

			template<typename T>
			static constexpr type_info create() noexcept
			{
				return type_info(type_name<T>(), type_hash<T>());
			}

			constexpr void make_empty() noexcept
			{
				name = type_name<empty_type_info_t>();
				hash = type_hash<empty_type_info_t>();
			}

			constexpr bool operator==(const type_info& other) noexcept
			{
				return (name == other.name && hash == other.hash);
			}

			std::string_view name;
			uint64_t hash;
		};

		template<typename...Types>
		constexpr size_t ptr_type_count_impl()
		{
			return (0 + ... + (std::is_pointer_v<Types> ? 1 : 0));
		}

		template<typename...Types>
		struct ptr_type_count
		{
			constexpr static size_t value = ptr_type_count_impl<Types...>();
		};

		template<typename...Types>
		struct ptr_type_count<std::tuple<Types...>>
		{
			constexpr static size_t value = ptr_type_count_impl<Types...>();
		};

		template<typename...Args>
		constexpr size_t lval_ref_type_count_impl()
		{
			return (0 + ... + (std::is_lvalue_reference_v<Args> ? 1 : 0));
		}

		template<typename...Args>
		struct lval_ref_type_count
		{
			constexpr static size_t value = lval_ref_type_count_impl<Args...>();
		};

		template<typename...Args>
		struct lval_ref_type_count<std::tuple<Args...>>
		{
			constexpr static size_t value = lval_ref_type_count_impl<Args...>();
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits {};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(*)(ArgTy...)>
		{		
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(&)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename ObjTy, typename...ArgTy>
		struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename T>
		struct function_traits<T> : function_traits<decltype(&T::operator())> {};

		template<size_t Start, size_t End>
		struct sub_tuple
		{
			static_assert(Start <= End, "Start index must be <= End");

			template<typename Tuple>
			using of = decltype([]<size_t... I>(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<Start + I, Tuple>...>
			{}(std::make_index_sequence<End - Start>{}));
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

		template <typename T>
		struct remove_suffix_cv {
			using type = T;
		};

		template <typename T>
		struct remove_suffix_cv<T const&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T volatile&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T const volatile&>
		{
			using type = T&;
		};

		template <typename T>
		struct remove_suffix_cv<T const*>
		{
			using type = T*;
		};

		template <typename T>
		struct remove_suffix_cv<T volatile*>
		{
			using type = T*;
		};

		template <typename T>
		struct remove_suffix_cv<T const volatile*>
		{
			using type = T*;
		};

		template <typename T>
		using remove_suffix_cv_t = typename remove_suffix_cv<T>::type;
	}
}
#endif 