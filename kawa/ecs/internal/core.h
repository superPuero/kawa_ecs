#ifndef KW_CORE
#define KW_CORE

#include <cstdint>
#include <cstddef>

#ifdef _DEBUG																			

#ifdef _MSC_VER
#include <intrin.h>
#define KW_DEBUG_BREAK() __debugbreak()
#elif __GNUC__ || __clang__
#define KW_DEBUG_BREAK() __builtin_trap()
#else
#define KW_DEBUG_BREAK() ((void)0)
#endif

#define KW_ASSERT(expr) if(!(expr)) KW_DEBUG_BREAK();

#define KW_ASSERT_MSG(expr, msg) \
        do { \
            if (!(expr)) { \
                std::cout << msg << '\n'; \
                KW_DEBUG_BREAK(); \
            } \
        } while(0)


#else

#define KW_ASSERT(expr) ((void)0)
#define KW_ASSERT_MSG(expr, msg) ((void)0)

#endif


#endif