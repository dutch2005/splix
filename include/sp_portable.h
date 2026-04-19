/*
 * 	    sp_portable.h             (C) 2024, SpliX Modernization Project
 *
 *  Portability layer for cross-platform support (Linux/Windows).
 * 
 */
#ifndef _SP_PORTABLE_H_
#define _SP_PORTABLE_H_

#include <cstring>
#include <ctime>

// --- Header Portability ---
#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)
    #include <io.h>
    #include <process.h>
    #define SP_PLATFORM_WINDOWS
#else
    #include <unistd.h>
    #define SP_PLATFORM_POSIX
#endif

// --- String Comparison Portability ---
#if defined(SP_PLATFORM_WINDOWS)
    #define SP_STRCASECMP _stricmp
    #define SP_STRNCASECMP _strnicmp
#else
    #include <strings.h>
    #define SP_STRCASECMP strcasecmp
    #define SP_STRNCASECMP strncasecmp
#endif

// --- Standard File Descriptors ---
#if !defined(STDIN_FILENO)
    #define SP_STDIN_FILENO 0
#else
    #define SP_STDIN_FILENO STDIN_FILENO
#endif

#if !defined(STDOUT_FILENO)
    #define SP_STDOUT_FILENO 1
#else
    #define SP_STDOUT_FILENO STDOUT_FILENO
#endif

// --- Thread-safe Time ---
namespace SP {
    inline struct tm* portable_localtime(const time_t* timer, struct tm* buf) {
#if defined(SP_PLATFORM_WINDOWS)
        if (localtime_s(buf, timer) == 0) return buf;
        return nullptr;
#else
        return localtime_r(timer, buf);
#endif
    }
}

#endif // _SP_PORTABLE_H_
