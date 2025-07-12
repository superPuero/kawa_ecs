#pragma once

#include <string>
#include <string_view>
#include <cstdint>
#include <utility>
#include <tuple>
#include <type_traits>

#if defined(__clang__)
#	define KW_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KW_META_TYPE_HAME_PREFIX "kawa::meta::type_name() [T = "
#	define KW_META_TYPE_HAME_POSTFIX "]"
#elif defined(_MSC_VER)
#	define KW_META_PRETTYFUNC __FUNCSIG__	
#	define KW_META_TYPE_HAME_PREFIX "kawa::meta::type_name<"
#	define KW_META_TYPE_HAME_POSTFIX ">(void) noexcept"
#elif defined(__GNUC__) 
#	define KW_META_PRETTYFUNC __PRETTY_FUNCTION__
#	define KW_META_TYPE_HAME_PREFIX "kawa::meta::type_name() [with T = "
#	define KW_META_TYPE_HAME_POSTFIX "; std::string_view = std::basic_string_view<char>]"
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
				size_t start = decorated_name.find(KW_META_TYPE_HAME_PREFIX);
				size_t end = decorated_name.find(KW_META_TYPE_HAME_POSTFIX);

				return decorated_name.substr(start + sizeof(KW_META_TYPE_HAME_PREFIX) - 1, end - (start + sizeof(KW_META_TYPE_HAME_PREFIX) - 1));
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
			return _internal::type_name_helper(KW_META_PRETTYFUNC);
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

		struct type_info
		{
			constexpr type_info(std::string_view n, uint64_t h) noexcept
				: name(n), hash(h) {}

			template<typename T>
			static constexpr type_info create() noexcept
			{
				return type_info(type_name<T>(), type_hash<T>());
			}

			std::string_view name;
			uint64_t hash;
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

		template<typename T>
		struct function_traits<T> : function_traits<decltype(&T::operator())> {};

		template<typename RetTy, typename ObjTy, typename...ArgTy>
		struct function_traits<RetTy(ObjTy::*)(ArgTy...) const>
		{
			using return_type = RetTy;
			using args_tuple = typename std::tuple<ArgTy...>;
			template<size_t i>
			using arg_at = typename std::tuple_element_t<i, args_tuple>;
		};

	}
}