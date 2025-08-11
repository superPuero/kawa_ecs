#ifndef KAWA_CORE
#define KAWA_CORE

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <format>
#include <chrono>

#include <format>
#include <iostream>

#define KAWA_ANSI_ESC_SEQ "\x1B"
#define KAWA_ANSI_COLOR_DEFAULT 39
#define KAWA_ANSI_BG_COLOR_DEFAULT 49

#define KAWA_ANSI_COLOR_YELLOW 33 
#define KAWA_ANSI_BG_COLOR_YELLOW 43 

#define KAWA_ANSI_COLOR_RED 31 
#define KAWA_ANSI_BG_COLOR_RED 41 

#define KAWA_ANSI_COLOR_BLACK 30 
#define KAWA_ANSI_BG_COLOR_BLACK 40

#define KAWA_ANSI_COLOR_WHITE 37 
#define KAWA_ANSI_BG_COLOR_WHITE 47

#ifndef KAWA_DISABLE_LOG

#define KAWA_LOG(...) \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT);\
		std::cout << "[KAWA LOG] " << std::format(__VA_ARGS__) << '\n';					  \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT)

#define KAWA_TODO(...) \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT);\
		std::cout << "[KAWA TODO] " << KAWA_FUNCNAME  << " "<< std::format(__VA_ARGS__) << '\n';					  \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_WHITE, KAWA_ANSI_BG_COLOR_BLACK)

#define KAWA_WARN(...) \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_YELLOW, KAWA_ANSI_BG_COLOR_DEFAULT);\
		std::cout << "[KAWA WARN] " << std::format(__VA_ARGS__) << '\n';					  \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT)

#define KAWA_ERROR(...) \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_RED, KAWA_ANSI_BG_COLOR_DEFAULT);\
		std::cout << "[KAWA ERROR] " << __FILE__ << " " << __func__ << " line " << __LINE__ << " " << std::format(__VA_ARGS__) << '\n';	\
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT);\
		KAWA_DEBUG_BREAK()

#define KAWA_PANIC(...) \
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_RED, KAWA_ANSI_BG_COLOR_DEFAULT);\
		std::cout << "[KAWA PANIC] " << __FILE__ << " " << __func__ << " line " << __LINE__ << " " << std::format(__VA_ARGS__) << '\n';	\
		std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT);\
		KAWA_DEBUG_BREAK()

#else
#define KAWA_LOG(...) 
#define KAWA_TODO(...)
#define KAWA_WARN(...)
#define KAWA_ERROR(...) 
#endif

#ifdef _MSC_VER
#define KAWA_ASSUME(x) __assume(x)
#elif __GNUC__ || __clang__
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
                KAWA_ERROR(__VA_ARGS__); \
            } \
        } while(0)

#ifdef _DEBUG	

#define KAWA_ASSERT(expr) if(!(expr)) KAWA_DEBUG_BREAK();

#define KAWA_ASSERT_MSG(expr, ...) \
        do { \
            if (!(expr)) { \
                std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_RED, KAWA_ANSI_BG_COLOR_DEFAULT);\
				std::cout << "[KAWA] " << "Assertion failed " << #expr << " file: " << __FILE__ << " func: " << __func__ << " line: " << __LINE__ << " " << std::format(__VA_ARGS__) << '\n';	\
				std::cout << std::format("{}[{};{}m", KAWA_ANSI_ESC_SEQ, KAWA_ANSI_COLOR_DEFAULT, KAWA_ANSI_BG_COLOR_DEFAULT);\
				KAWA_DEBUG_BREAK();\
            } \
        } while(0)

#define KAWA_DEBUG_EXPAND(x) x

#else

#define KAWA_ASSERT(expr) ((void)0)
#define KAWA_ASSERT_MSG(expr, ...) ((void)0)
#define KAWA_DEBUG_EXPAND(x) ((void)0)

#endif

#endif