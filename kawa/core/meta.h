#ifndef KAWA_META
#define KAWA_META

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <tuple>
#include <type_traits>

#include "core_types.h"

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
		namespace _
		{
			template<typename T>
			extern const T fake_object;

			inline constexpr kawa::string_view type_name_helper(kawa::string_view decorated_name)
			{
				usize start = decorated_name.find(KAWA_META_TYPE_NAME_PREFIX);
				usize end = decorated_name.find(KAWA_META_TYPE_NAME_POSTFIX);

				auto semi_clean_name = decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
			
//#ifdef _MSC_VER	
//
//				auto pre_name = decorated_name.substr(start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1, end - (start + sizeof(KAWA_META_TYPE_NAME_PREFIX) - 1));
//				usize pre_name_space = pre_name.find_first_of(' ');				
//
//				if (pre_name_space != std::string::npos)
//				{
//					return pre_name.substr(pre_name_space + 1, pre_name.size() - pre_name_space);
//				}
//				return pre_name;
//#else
				return semi_clean_name;
//#endif 

			}

			constexpr u64 fnv1a_hash(kawa::string_view str) noexcept
			{
				constexpr u64 fnv_offset_basis = 14695981039346656037ull;
				constexpr u64 fnv_prime = 1099511628211ull;

				u64 hash = fnv_offset_basis;

				for (char c : str)
				{
					hash ^= static_cast<u64>(c);
					hash *= fnv_prime;
				}

				return hash;
			}
		}

		template<typename T>
		inline consteval kawa::string_view type_name() noexcept
		{
			//return KAWA_META_PRETTYFUNC;
			return _::type_name_helper(KAWA_META_PRETTYFUNC);
		}

		constexpr u64 string_hash(string_view str) noexcept					
		{
			return _::fnv1a_hash(str);
		}

		template<typename T>
		inline consteval u64 type_hash() noexcept
		{
			return string_hash(type_name<T>());
		}

		template<typename T>
		struct construct_tag{};

		struct type_info
		{
			inline constexpr type_info() noexcept
				: name("")
				, hash(0)
				, size(0)
				, alignment(0)
			{
			}

			template<typename T>
			inline constexpr type_info(construct_tag<T>)
				: name(type_name<T>())
				, hash(type_hash<T>())
				, size(sizeof(T))
				, alignment(alignof(T))
			{
			}

			template<typename T>
			void refresh() noexcept
			{
				*this = type_info(construct_tag<T>{});
			}

			constexpr operator bool() const noexcept
			{
				return hash;
			}

			constexpr bool operator==(const type_info& other) const noexcept
			{
				return (name == other.name && hash == other.hash);
			}

			template<typename T>
			constexpr inline bool is() const noexcept
			{
				return hash == type_hash<T>();
			}

			void reset()
			{
				*this = type_info();
			}

			kawa::string_view name;
			u64 hash;
			usize size;
			usize alignment;
		};
		template <template <typename> class Predicate, typename Tuple>
		struct filter_tuple;

		template <template <typename> class Predicate, typename... Ts>
		struct filter_tuple<Predicate, std::tuple<Ts...>> {
			
			using type = decltype(std::tuple_cat(
				std::declval<
				typename std::conditional<
				Predicate<Ts>::value, 
				std::tuple<Ts>,       
				std::tuple<>          
				>::type
				>()...
			));
		};

		template <template <typename> class Predicate, typename Tuple>
		using filter_tuple_t = typename filter_tuple<Predicate, Tuple>::type;

		template<typename RetTy, typename...ArgTy>
		struct function_traits {};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(*)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<usize i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(&)(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<usize i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename...ArgTy>
		struct function_traits<RetTy(ArgTy...)>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<usize i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename RetTy, typename ObjTy, typename...ArgTy>
		struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<usize i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

		template<typename T>
		struct function_traits<T> : function_traits<decltype(&T::operator())> {};

		template<typename Tuple, usize start, usize end, typename = void>
		struct sub_tuple
		{
			using tuple = decltype([]<usize... I>(std::index_sequence<I...>) -> std::tuple<std::tuple_element_t<start + I, Tuple>...>
			{
			}(std::make_index_sequence<end - start>{}));
		};

		template<typename Tuple, usize start, usize end>
		struct sub_tuple<Tuple, start, end, std::enable_if_t<((end - start) > std::tuple_size_v<Tuple>)>>
		{
			using tuple = typename std::tuple<>;
		};

		template <template <typename> typename F, typename T>
		struct transform_each;

		template <template <typename> typename F, typename...Types>
		struct transform_each<F, std::tuple<Types...>>
		{
			using type = typename std::tuple<F<Types>...>;
		};

		template<template <typename...> typename F, typename T>
		using transform_each_t = typename transform_each<F, T>::type;
	}
}
#endif 