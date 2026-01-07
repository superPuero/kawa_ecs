#ifndef KAWA_PLATFORM_DETECT
#define KAWA_PLATFORM_DETECT

#ifdef kw_platform_windows
#define kw_platform windows
#endif

#ifdef kw_platform_linux
#define kw_platform linux
#endif

#ifdef kw_platform_macos
#define kw_platform macOS
#endif

#endif KAWA_PLATFORM_DETECT