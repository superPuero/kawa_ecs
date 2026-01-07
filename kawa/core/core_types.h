#ifndef KAWA_TYPES
#define KAWA_TYPES

#include <utility>
#include <cstdint>
#include <vector>
#include <array>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <future>
#include <atomic>
#include <thread>
#include <string_view>
#include <new>
#include <ranges>
#include <type_traits>

#include "macros.h"

namespace kawa
{
	using opaque_ptr = void*;

	using u8	= uint8_t;
	using i8	= int8_t;

	using u16	= uint16_t;
	using i16	= int16_t;

	using u32	= uint32_t;
	using i32	= int32_t;

	using u64	= uint64_t;
	using i64	= int64_t;

	using usize = size_t;
	using isize = ptrdiff_t;

	using f32 = float;
	using f64 = double;
					
	using string = std::string;
	using string_view = std::string_view;

	using bool8 = u8;
	using bool16 = u16;
	using bool32 = u32;
	using bool64 = u64;

	template<typename T1, typename T2>
	using pair = std::pair<T1, T2>;

	template<typename...Args>
	using tuple = std::tuple<Args...>;

	template<typename T>
	using set = std::set<T>;

	template<typename T>
	using uset = std::unordered_set<T>;

	template<typename Key, typename Val>
	using map = std::map<Key, Val>;

	template<typename Key, typename Val>
	using umap = std::unordered_map<Key, Val>;

	template<typename T>
	using dyn_array = std::vector<T>;

	template<typename T, usize size>
	using array = std::array<T, size>;

	using thread = std::thread;

	template<typename T>
	using unique = std::unique_ptr<T>;

	template<typename T>
	using atomic = std::atomic<T>;

}

#endif // !KAWA_TYPES
