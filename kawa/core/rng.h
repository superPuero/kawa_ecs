#ifndef KAWA_RNG
#define KAWA_RNG

#include <cstdint>
#include <random>

#include "core_types.h"

namespace kawa
{
class rng
{
public:
	[[nodiscard]] static inline u32 randu32() noexcept
	{
		return _dist32(_generator32);
	}

	[[nodiscard]] static inline u64 randu64() noexcept
	{
		return _dist64(_generator64);
	}

	[[nodiscard]] static inline f32 randf32(f32 from = 0.0f, f32 to = 1.0f) noexcept
	{
		return _distf32(_generator32) * (to - from) + from;
	}

	[[nodiscard]] static inline f64 randf64(f64 from = 0.0f, f64 to = 0.1f) noexcept
	{
		return _distf64(_generator64) * (to - from) + from;
	}

private:
	inline thread_local static std::mt19937 _generator32{ std::random_device{}() };
	inline thread_local static std::uniform_int_distribution<u32> _dist32;

	inline thread_local static std::mt19937_64 _generator64{ std::random_device{}() };
	inline thread_local static std::uniform_int_distribution<u64> _dist64;

	inline thread_local static std::uniform_real_distribution<f32> _distf32{ 0.0f, 1.0f };
	inline thread_local static std::uniform_real_distribution<f64> _distf64{ 0.0f, 1.0f };
};
}

#endif
