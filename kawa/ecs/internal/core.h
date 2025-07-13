#ifndef KAWA_CORE
#define KAWA_CORE

#include <cstdint>
#include <cstddef>

#ifdef _DEBUG																			

#ifdef _MSC_VER
#include <intrin.h>
#define KAWA_DEBUG_BREAK() __debugbreak()
#elif __GNUC__ || __clang__
#define KAWA_DEBUG_BREAK() __builtin_trap()
#else
#define KAWA_DEBUG_BREAK() ((void)0)
#endif

#define KAWA_ASSERT(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                std::cout << msg << '\n'; \
                KAWA_DEBUG_BREAK(); \
            } \
        } while(0)


#else

#define KAWA_ASSERT(expr) ((void)0)
#define KAWA_ASSERT_MSG(expr, msg) ((void)0)

#endif


#endif