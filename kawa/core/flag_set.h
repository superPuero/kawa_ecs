#ifndef KAWA_FLAG_SET
#define KAWA_FLAG_SET

#include <concepts>

namespace kawa
{
	template<typename T>
		requires std::is_enum_v<T>
	struct flag_set
	{
		using flag_type = std::underlying_type_t<T>;

		template<typename...Args>
		constexpr flag_set(Args...flags) noexcept
		{
			set(flags...);
		}

		template<typename...Args>
		constexpr void set(Args...flags) noexcept
		{
			((value |= static_cast<flag_type>(flags)), ...);
		}
	
		template<typename...Args>
		constexpr flag_set<T>& operator=(Args... flags) noexcept
		{
			set(flags...);
			return *this;
		}

		constexpr bool has(T flag) const noexcept
		{
			return value & static_cast<flag_type>(flag);
		}

		flag_type value{};
	};
}

#endif