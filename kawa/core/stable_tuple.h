#ifndef KAWA_STABLE_TUPLE
#define KAWA_STABLE_TUPLE

#include "core_types.h"

namespace kawa
{
	template<typename...args_t>
	struct stable_tuple;

	template<typename...Args>
	constexpr stable_tuple<Args&&...> forward_as_tuple(Args&&...args) noexcept
	{
		return stable_tuple<Args&&...>(std::forward<Args>(args)...);
	}

	template<>
	struct stable_tuple<>
	{
		template<usize index>
		struct arg_at { static_assert(false); };

		template<usize index>
		using arg_at_t = arg_at<index>::type;

		constexpr static usize size = 0;

		constexpr static auto index_sequence = std::make_index_sequence<size>{};

		template<usize i>
		decltype(auto) get() noexcept
		{
			static_assert(false, "stable tuple index out of bounds");
		}

		template<usize i>
		decltype(auto) get() const noexcept
		{
			static_assert(false, "stable tuple index out of bounds");
		}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) noexcept {}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) const noexcept {}

		const void* data() const noexcept
		{
			return nullptr;
		}

		stable_tuple() noexcept = default;
	};

	template<typename head_t>
	struct stable_tuple<head_t>
	{
		template<usize index>
		struct arg_at { static_assert(false); };

		template<>
		struct arg_at<0> { using type = head_t; };

		template<usize index>
		using arg_at_t = arg_at<index>::type;

		constexpr static usize size = 1;

		constexpr static auto index_sequence = std::make_index_sequence<size>{};

		stable_tuple() noexcept = default;

		template<typename head_arg_t>
		stable_tuple(head_arg_t&& head_arg) noexcept
			: head(std::forward<head_arg_t>(head_arg))
		{
		}

		template<usize i>
		auto& get() noexcept
		{
			if constexpr (i == 0)
			{
				return head;
			}
			else
			{
				static_assert(false, "stable tuple index out of bounds");
			}
		}

		template<usize i>
		const auto& get() const noexcept
		{
			if constexpr (i == 0)
			{
				return head;
			}
			else
			{
				static_assert(false, "stable tuple index out of bounds");
			}
		}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) noexcept
		{
			fn(head);
		}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) const noexcept
		{
			fn(head);
		}

		const void* data() const noexcept
		{
			return &head;
		}

		head_t head;
	};	

	template<typename head_t, typename...rest_t>
	struct stable_tuple<head_t, rest_t...>
	{
		using rest_tuple_t = stable_tuple<rest_t...>;

		template<usize index>
		struct arg_at { using type = rest_tuple_t::template arg_at<index - 1>::type; };

		template<>
		struct arg_at<0> { using type = head_t; };

		template<usize index>
		using arg_at_t = arg_at<index>::type;

		constexpr static usize size = 1 + sizeof...(rest_t);

		constexpr static auto index_sequence = std::make_index_sequence<size>{};

		stable_tuple() noexcept = default;

		template<typename head_arg_t, typename...rest_args_t>
		stable_tuple(head_arg_t&& head_arg, rest_args_t&&...rest_args) noexcept
			: head(std::forward<head_arg_t>(head_arg))
			, rest(std::forward<rest_args_t>(rest_args)...)
		{

		}

		template<usize i>
		auto& get() noexcept
		{
			if constexpr (i == 0)
			{
				return head;
			}
			else
			{
				return rest.get<i - 1>();
			}
		}

		template<usize i>
		const auto& get() const noexcept
		{
			if constexpr (i == 0)
			{
				return head;
			}
			else
			{
				return rest.get<i - 1>();
			}
		}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) noexcept
		{
			fn(head);
			rest.for_each(std::forward<for_each_fn_t>(fn));
		}

		template<typename for_each_fn_t>
		constexpr void for_each(for_each_fn_t&& fn) const noexcept
		{
			fn(head);
			rest.for_each(std::forward<for_each_fn_t>(fn));
		}

		const void* data() const noexcept
		{
			return &head;
		}

		[[no_unique_address]] head_t head;
		[[no_unique_address]] rest_tuple_t rest;
	};	

	template<typename...Args>
	stable_tuple(Args...args) -> stable_tuple<Args...>;
}


template<typename...Args>
struct std::tuple_size<kawa::stable_tuple<Args...>> : std::integral_constant<size_t, kawa::stable_tuple<Args...>::size> {};

template<size_t I, typename... Args>
struct std::tuple_element<I, kawa::stable_tuple<Args...>> : kawa::stable_tuple<Args...>::template arg_at<I> {};

#endif