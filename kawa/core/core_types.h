#ifndef KAWA_CORE_TYPES
#define KAWA_CORE_TYPES

#include <cstdint>
#include <vector>
#include <array>
#include <string>

namespace kawa
{
	using opaque_ptr = void*;

	using u8 = uint8_t;
	using i8 = int8_t;

	using u16 = uint16_t;
	using i16 = int16_t;

	using u32 = uint32_t;
	using i32 = int32_t;

	using u64 = uint64_t;
	using i64 = int64_t;

	using usize = size_t;
	using isize = ptrdiff_t;

	using string = std::string;

	template<typename T>
	using dyn_array = std::vector<T>;

	template<typename T, usize size>
	using array = std::array<T, size>;

	using f32 = float;
	using f64 = double;
}

#endif