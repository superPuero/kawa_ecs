#ifndef KAWA_CORE
#define KAWA_CORE

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <format>
#include <chrono>

#if defined(_MSC_VER)
#define KAWA_ASSUME(x) __assume(x)
#elif defined(__clang__) || defined(__GNUC__)
#define KAWA_ASSUME(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#else
#define KAWA_ASSUME(x) ((void)0)
#endif


#ifdef _MSC_VER
#include <intrin.h>
#define KAWA_DEBUG_BREAK() __debugbreak()
#elif __GNUC__ || __clang__
#define KAWA_DEBUG_BREAK() __builtin_trap()
#else
#define KAWA_DEBUG_BREAK() std::terminate() 
#endif

#define KAWA_VERIFY(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_VERIFY_MSG(expr, ...) \
        do { \
            if (!(expr)) { \
                std::cout << std::format(__VA_ARGS__) << '\n'; \
                KAWA_DEBUG_BREAK(); \
            } \
        } while(0)

#ifdef _DEBUG	

#define KAWA_ASSERT(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_ASSERT_MSG(expr, ...) \
        do { \
            if (!(expr)) { \
                std::cout << std::format(__VA_ARGS__) << '\n'; \
                KAWA_DEBUG_BREAK(); \
            } \
        } while(0)

#define KAWA_DEBUG_EXPAND(x) x

#else

#define KAWA_ASSERT(expr) ((void)0)
#define KAWA_ASSERT_MSG(expr, ...) ((void)0)
#define KAWA_DEBUG_EXPAND(x) ((void)0)

#endif

#endif