#ifndef KAWA_MACROS
#define KAWA_MACROS

#include <cstdint>
#include <cstddef>
#include <iostream>
#include <format>
#include <chrono>

#include "platform_detect.h"

#ifdef _MSC_VER
#define kw_assume(x) __assume(x)
#elif __GNUC__ || __clang__
#define kw_assume(x) do { if (!(x)) __builtin_unreachable(); } while(0)
#else
#define kw_assume(x) ((void)0)
#endif


#ifdef _MSC_VER
#include <intrin.h>
#define kw_debugbreak() __debugbreak()
#elif __GNUC__ || __clang__
#define kw_debugbreak() __builtin_trap()
#else
#define kw_debugbreak() std::terminate() 
#endif


#define kw_stringify(x) #x
#define kw_expand_stringify(x) kw_stringify(x)

#define kw_expand(...)  kw_expand1(kw_expand1(kw_expand1(kw_expand1(__VA_ARGS__))))
#define kw_expand1(...) kw_expand2(kw_expand2(kw_expand2(kw_expand2(__VA_ARGS__))))
#define kw_expand2(...) kw_expand3(kw_expand3(kw_expand3(kw_expand3(__VA_ARGS__))))
#define kw_expand3(...) kw_expand4(kw_expand4(kw_expand4(kw_expand4(__VA_ARGS__))))
#define kw_expand4(...) __VA_ARGS__

#ifdef kw_debug
#define kw_build debug
#endif

#ifdef kw_release
#define kw_build release
#endif

#ifdef kw_dist
#define kw_build dist
#endif

#ifndef kw_build
#define kw_build unspecified
#endif
#define kw_build_str kw_expand_stringify(kw_build)
#define kw_platform_str kw_expand_stringify(kw_platform)

#define kw_parens ()

#define kw_c_array_size(arr) (sizeof(arr) / sizeof(arr[0]))

#define kw_bit(bit) (1u << (bit))

#define kw_for_each(macro, ...) __VA_OPT__(kw_expand(kw_for_each_helper(macro, __VA_ARGS__)))

#define kw_for_each_helper(macro, v1, ...) \
macro(v1)\
__VA_OPT__(kw_for_each_again kw_parens (macro, __VA_ARGS__))\

#define kw_for_each_again() kw_for_each_helper

#define kw_alias(name, type)\
struct name \
{ \
	name(const type& v) : value(v) {};\
	type value;\
}

#define kw_ansi_esc_sequence "\x1B"

#define kw_ansi_color_black   30
#define kw_ansi_color_red     31
#define kw_ansi_color_green   32
#define kw_ansi_color_yellow  33
#define kw_ansi_color_blue    34
#define kw_ansi_color_magenta 35
#define kw_ansi_color_cyan    36
#define kw_ansi_color_white   37

#define kw_ansi_color_bright_black   90
#define kw_ansi_color_bright_red     91
#define kw_ansi_color_bright_green   92
#define kw_ansi_color_bright_yellow  93
#define kw_ansi_color_bright_blue    94
#define kw_ansi_color_bright_magenta 95
#define kw_ansi_color_bright_cyan    96
#define kw_ansi_color_bright_white   97

#define kw_ansi_background_color_black   40
#define kw_ansi_background_color_red     41
#define kw_ansi_background_color_green   42
#define kw_ansi_background_color_yellow  43
#define kw_ansi_background_color_blue    44
#define kw_ansi_background_color_magenta 45
#define kw_ansi_background_color_cyan    46
#define kw_ansi_background_color_white   47

#define kw_ansi_background_color_bright_black   100
#define kw_ansi_background_color_bright_red     101
#define kw_ansi_background_color_bright_green   102
#define kw_ansi_background_color_bright_yellow  103
#define kw_ansi_background_color_bright_blue    104
#define kw_ansi_background_color_bright_magenta 105
#define kw_ansi_background_color_bright_cyan    106
#define kw_ansi_background_color_bright_white   107

#define kw_ansi_color_default 39
#define kw_ansi_background_color_default 49

#ifndef kw_log_handle
#define kw_log_handle stdout
#endif

#define kw_put(dest, fmt, ...)  std::fputs(std::format(fmt, __VA_ARGS__).c_str(), dest)

#define kw_print(fmt, ...) kw_put(kw_log_handle, fmt, __VA_ARGS__)
#define kw_println(fmt, ...) kw_put(kw_log_handle, fmt, __VA_ARGS__), kw_put(kw_log_handle, "{}", "\n")

#define kw_print_ansi_cfg(text_color_ansi_code, background_color_ansi_code) kw_print("{}[{};{}m", kw_ansi_esc_sequence, text_color_ansi_code, background_color_ansi_code)

#define kw_print_colored(text_color_ansi_code, background_color_ansi_code, fmt, ...) kw_print_ansi_cfg(text_color_ansi_code, background_color_ansi_code); kw_print(fmt, __VA_ARGS__); kw_print_ansi_cfg(kw_ansi_color_default, kw_ansi_background_color_default) 
#define kw_println_colored(text_color_ansi_code, background_color_ansi_code, fmt, ...) kw_print_ansi_cfg(text_color_ansi_code, background_color_ansi_code); kw_println(fmt, __VA_ARGS__); kw_print_ansi_cfg(kw_ansi_color_default, kw_ansi_background_color_default) 


#define kw_print_with_colored_label(text_color_ansi_code, text_background_color_ansi_code, label, fmt, ...)\
kw_print_ansi_cfg(text_color_ansi_code, text_background_color_ansi_code);\
kw_print("{} ", #label);\
kw_print_ansi_cfg(kw_ansi_color_default, kw_ansi_background_color_default);\
kw_print(fmt, __VA_ARGS__)

#define kw_println_with_colored_label(text_color_ansi_code, text_background_color_ansi_code, label, fmt, ...)\
kw_print_ansi_cfg(text_color_ansi_code, text_background_color_ansi_code);\
kw_print("{} ", #label);\
kw_print_ansi_cfg(kw_ansi_color_default, kw_ansi_background_color_default);\
kw_println(fmt, __VA_ARGS__)


#define kw_info(fmt, ...) kw_println_with_colored_label(kw_ansi_color_green, kw_ansi_background_color_default, info, fmt, __VA_ARGS__)
#define kw_info_expr(var) kw_info("{}: {}", #var, var)
#define kw_warn(fmt, ...) kw_println_with_colored_label(kw_ansi_color_yellow, kw_ansi_background_color_default, warn, fmt, __VA_ARGS__)
#define kw_error(fmt, ...) kw_println_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, error, fmt, __VA_ARGS__); kw_debugbreak()

#define kw_verify(expr)\
		do { \
            if (!(expr)) { \
				kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, verify, "[{}] failed", #expr); \
				kw_debugbreak();\
            } \
        } while(0)

#define kw_verify_msg(expr, fmt, ...) \
        do { \
            if (!(expr)) { \
                kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, verify, fmt, __VA_ARGS__);\
				kw_debugbreak();\
            } \
        } while(0)

#define kw_panic()\
				kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, panic, "{}", "encountered"); \
				kw_debugbreak();

#define kw_panic_msg(fmt, ...) \
				kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, panic, fmt, __VA_ARGS__);\
				kw_debugbreak()

#ifdef kw_debug
#define kw_assert(expr)\
		do { \
            if (!(expr)) { \
				kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, assertion, "[{}] failed", #expr); \
				kw_debugbreak();\
            } \
        } while(0)

#define kw_assert_msg(expr, fmt, ...) \
        do { \
            if (!(expr)) { \
                kw_print_with_colored_label(kw_ansi_color_red, kw_ansi_background_color_default, assertion, fmt, __VA_ARGS__);\
				kw_debugbreak();\
            } \
        } while(0)

#define kw_debug_expand(x) x
#else
#define kw_assert(expr) ((void)0)
#define kw_assert_msg(expr, ...) ((void)0)

#define kw_debug_expand(x) ((void)0)
#endif

#endif