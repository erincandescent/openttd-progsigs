/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file stdafx.h Definition of base types and functions in a cross-platform compatible way. */

#ifndef STDAFX_H
#define STDAFX_H

#if defined(__APPLE__)
	#include "os/macosx/osx_stdafx.h"
#endif /* __APPLE__ */

#if defined(__BEOS__) || defined(__HAIKU__)
	#include <SupportDefs.h>
	#include <unistd.h>
	#define _GNU_SOURCE
	#define TROUBLED_INTS
#elif defined(__NDS__)
	#include <nds/jtypes.h>
	#define TROUBLED_INTS
#endif

/* It seems that we need to include stdint.h before anything else
 * We need INT64_MAX, which for most systems comes from stdint.h. However, MSVC
 * does not have stdint.h and apparently neither does MorphOS, so define
 * INT64_MAX for them ourselves. */
#if defined(__APPLE__)
	/* Already done in osx_stdafx.h */
#elif !defined(_MSC_VER) && !defined( __MORPHOS__) && !defined(_STDINT_H_)
	#if defined(SUNOS)
		/* SunOS/Solaris does not have stdint.h, but inttypes.h defines everything
		 * stdint.h defines and we need. */
		#include <inttypes.h>
	# else
		#define __STDC_LIMIT_MACROS
		#include <stdint.h>
	#endif
#else
	#define UINT64_MAX (18446744073709551615ULL)
	#define INT64_MAX  (9223372036854775807LL)
	#define INT64_MIN  (-INT64_MAX - 1)
	#define UINT32_MAX (4294967295U)
	#define INT32_MAX  (2147483647)
	#define INT32_MIN  (-INT32_MAX - 1)
	#define UINT16_MAX (65535U)
	#define INT16_MAX  (32767)
	#define INT16_MIN  (-INT16_MAX - 1)
	#define UINT8_MAX  (255)
	#define INT8_MAX   (127)
	#define INT8_MIN   (-INT8_MAX - 1)
#endif

#include <cstdio>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <climits>
#include <cassert>

#if defined(UNIX) || defined(__MINGW32__)
	#include <sys/types.h>
#endif

#if defined(__OS2__)
	#include <types.h>
	#define strcasecmp stricmp
#endif

#if defined(PSP)
	#include <psptypes.h>
	#include <pspdebug.h>
	#include <pspthreadman.h>
#endif

#if defined(SUNOS) || defined(HPUX)
	#include <alloca.h>
#endif

#if defined(__MORPHOS__)
	/* MorphOS defines certain Amiga defines per default, we undefine them
	 * here to make the rest of source less messy and more clear what is
	 * required for morphos and what for AmigaOS */
	#if defined(amigaos)
		#undef amigaos
	#endif
	#if defined(__amigaos__)
		#undef __amigaos__
	# endif
	#if defined(__AMIGA__)
		#undef __AMIGA__
	#endif
	#if defined(AMIGA)
		#undef AMIGA
	#endif
	#if defined(amiga)
		#undef amiga
	#endif
	/* Act like we already included this file, as it somehow gives linkage problems
	 *  (mismatch linkage of C++ and C between this include and unistd.h). */
	#define CLIB_USERGROUP_PROTOS_H
#endif /* __MORPHOS__ */

#if defined(PSP)
	/* PSP can only have 10 file-descriptors open at any given time, but this
	 *  switch only limits reads via the Fio system. So keep 2 fds free for things
	 *  like saving a game. */
	#define LIMITED_FDS 8
	#define printf pspDebugScreenPrintf
#endif /* PSP */

/* Stuff for GCC */
#if defined(__GNUC__)
	#define NORETURN __attribute__ ((noreturn))
	#define FORCEINLINE inline
	#define CDECL
	#define __int64 long long
	#define GCC_PACK __attribute__((packed))
	/* Warn about functions using 'printf' format syntax. First argument determines which parameter
	 * is the format string, second argument is start of values passed to printf. */
	#define WARN_FORMAT(string, args) __attribute__ ((format (printf, string, args)))
#endif /* __GNUC__ */

#if defined(__WATCOMC__)
	#define NORETURN
	#define FORCEINLINE inline
	#define CDECL
	#define GCC_PACK
	#define WARN_FORMAT(string, args)
	#include <malloc.h>
#endif /* __WATCOMC__ */

#if defined(__MINGW32__) || defined(__CYGWIN__)
	#include <malloc.h> // alloca()
#endif

#if defined(WIN32)
	#define WIN32_LEAN_AND_MEAN     // Exclude rarely-used stuff from Windows headers
#endif

/* Stuff for MSVC */
#if defined(_MSC_VER)
	#pragma once
	/* Define a win32 target platform, to override defaults of the SDK
	 * We need to define NTDDI version for Vista SDK, but win2k is minimum */
	#define NTDDI_VERSION NTDDI_WIN2K // Windows 2000
	#define _WIN32_WINNT 0x0500       // Windows 2000
	#define _WIN32_WINDOWS 0x400      // Windows 95
	#if !defined(WINCE)
		#define WINVER 0x0400     // Windows NT 4.0 / Windows 95
	#endif
	#define _WIN32_IE_ 0x0401         // 4.01 (win98 and NT4SP5+)

	#pragma warning(disable: 4244)  // 'conversion' conversion from 'type1' to 'type2', possible loss of data
	#pragma warning(disable: 4761)  // integral size mismatch in argument : conversion supplied
	#pragma warning(disable: 4200)  // nonstandard extension used : zero-sized array in struct/union

	#if (_MSC_VER < 1400)                   // MSVC 2005 safety checks
		#error "Only MSVC 2005 or higher are supported. MSVC 2003 and earlier are not! Upgrade your compiler."
	#endif /* (_MSC_VER < 1400) */
	#pragma warning(disable: 4291)   // no matching operator delete found; memory will not be freed if initialization throws an exception (reason: our overloaded functions never throw an exception)
	#pragma warning(disable: 4996)   // 'strdup' was declared deprecated
	#define _CRT_SECURE_NO_DEPRECATE // all deprecated 'unsafe string functions
	#pragma warning(disable: 6308)   // code analyzer: 'realloc' might return null pointer: assigning null pointer to 't_ptr', which is passed as an argument to 'realloc', will cause the original memory block to be leaked
	#pragma warning(disable: 6011)   // code analyzer: Dereferencing NULL pointer 'pfGetAddrInfo': Lines: 995, 996, 998, 999, 1001
	#pragma warning(disable: 6326)   // code analyzer: potential comparison of a constant with another constant
	#pragma warning(disable: 6031)   // code analyzer: Return value ignored: 'ReadFile'
	#pragma warning(disable: 6255)   // code analyzer: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
	#pragma warning(disable: 6246)   // code analyzer: Local declaration of 'statspec' hides declaration of the same name in outer scope. For additional information, see previous declaration at ...
	#define WARN_FORMAT(string, args)

	#include <malloc.h> // alloca()
	#define NORETURN __declspec(noreturn)
	#define FORCEINLINE __forceinline
	#define inline _inline

	#if !defined(WINCE)
		#define CDECL _cdecl
	#endif

	#define GCC_PACK

	int CDECL snprintf(char *str, size_t size, const char *format, ...) WARN_FORMAT(3, 4);
	#if defined(WINCE)
		int CDECL vsnprintf(char *str, size_t size, const char *format, va_list ap);
	#endif

	#if defined(WIN32) && !defined(_WIN64) && !defined(WIN64)
		#if !defined(_W64)
			#define _W64
		#endif

		typedef _W64 int INT_PTR, *PINT_PTR;
		typedef _W64 unsigned int UINT_PTR, *PUINT_PTR;
	#endif /* WIN32 && !_WIN64 && !WIN64 */

	#if defined(_WIN64) || defined(WIN64)
		#define fseek _fseeki64
	#endif /* _WIN64 || WIN64 */

	/* This is needed to zlib uses the stdcall calling convention on visual studio */
	#if defined(WITH_ZLIB) || defined(WITH_PNG)
		#if !defined(ZLIB_WINAPI)
			#define ZLIB_WINAPI
		#endif
	#endif

	#if defined(WINCE)
		#define strcasecmp _stricmp
		#define strncasecmp _strnicmp
		#undef DEBUG
	#else
		#define strcasecmp stricmp
		#define strncasecmp strnicmp
	#endif

	/* MSVC doesn't have these :( */
	#define S_ISDIR(mode) (mode & S_IFDIR)
	#define S_ISREG(mode) (mode & S_IFREG)

#endif /* defined(_MSC_VER) */

#if defined(DOS)
	/* The DOS port does not have all signals/signal functions. */
	#define strsignal(sig) ""
	/* Use 'no floating point' for bus errors; SIGBUS does not exist
	 * for DOS, SIGNOFP for other platforms. So it's fairly safe
	 * to interchange those. */
	#define SIGBUS SIGNOFP
#endif

#if defined(WINCE)
	#define strdup _strdup
#endif /* WINCE */

/* NOTE: the string returned by these functions is only valid until the next
 * call to the same function and is not thread- or reentrancy-safe */
#if !defined(STRGEN)
	#if defined(WIN32) || defined(WIN64)
		char *getcwd(char *buf, size_t size);
		#include <tchar.h>
		#include <io.h>

		/* XXX - WinCE without MSVCRT doesn't support wfopen, so it seems */
		#if !defined(WINCE)
			#define fopen(file, mode) _tfopen(OTTD2FS(file), _T(mode))
			#define unlink(file) _tunlink(OTTD2FS(file))
		#endif /* WINCE */

		const char *FS2OTTD(const TCHAR *name);
		const TCHAR *OTTD2FS(const char *name);
		#define SQ2OTTD(name) FS2OTTD(name)
		#define OTTD2SQ(name) OTTD2FS(name)
	#else
		#define fopen(file, mode) fopen(OTTD2FS(file), mode)
		const char *FS2OTTD(const char *name);
		const char *OTTD2FS(const char *name);
		#define SQ2OTTD(name) (name)
		#define OTTD2SQ(name) (name)
	#endif /* WIN32 */
#endif /* STRGEN */

#if defined(WIN32) || defined(WIN64) || defined(__OS2__) && !defined(__INNOTEK_LIBC__)
	#define PATHSEP "\\"
	#define PATHSEPCHAR '\\'
#else
	#define PATHSEP "/"
	#define PATHSEPCHAR '/'
#endif

/* MSVCRT of course has to have a different syntax for long long *sigh* */
#if defined(_MSC_VER) || defined(__MINGW32__)
	#define OTTD_PRINTF64 "%I64d"
	#define OTTD_PRINTFHEX64 "%I64x"
	#define PRINTF_SIZE "%Iu"
#else
	#define OTTD_PRINTF64 "%lld"
	#define OTTD_PRINTFHEX64 "%llx"
	#define PRINTF_SIZE "%zu"
#endif

typedef unsigned char byte;

/* This is already defined in unix, but not in QNX Neutrino (6.x)*/
#if (!defined(UNIX) && !defined(__CYGWIN__) && !defined(__BEOS__) && !defined(__HAIKU__) && !defined(__MORPHOS__)) || defined(__QNXNTO__)
	typedef unsigned int uint;
#endif

#if defined(TROUBLED_INTS)
	/* NDS'/BeOS'/Haiku's types for uint32/int32 are based on longs, which causes
	 * trouble all over the place in OpenTTD. */
	#define uint32 uint32_ugly_hack
	#define int32 int32_ugly_hack
	typedef unsigned int uint32_ugly_hack;
	typedef signed int int32_ugly_hack;
#else
	typedef unsigned char    uint8;
	typedef   signed char     int8;
	typedef unsigned short   uint16;
	typedef   signed short    int16;
	typedef unsigned int     uint32;
	typedef   signed int      int32;
	typedef unsigned __int64 uint64;
	typedef   signed __int64  int64;
#endif /* !TROUBLED_INTS */

#if !defined(WITH_PERSONAL_DIR)
	#define PERSONAL_DIR ""
#endif

/* Compile time assertions. Prefer c++0x static_assert().
 * Older compilers cannot evaluate some expressions at compile time,
 * typically when templates are involved, try assert_tcompile() in those cases. */
#if defined(__STDCXX_VERSION__) || defined(__GXX_EXPERIMENTAL_CXX0X__) || defined(__GXX_EXPERIMENTAL_CPP0X__) || defined(static_assert)
	/* __STDCXX_VERSION__ is c++0x feature macro, __GXX_EXPERIMENTAL_CXX0X__ is used by gcc, __GXX_EXPERIMENTAL_CPP0X__ by icc */
	#define assert_compile(expr) static_assert(expr, #expr )
	#define assert_tcompile(expr) assert_compile(expr)
#elif defined(__OS2__)
	/* Disabled for OS/2 */
	#define assert_compile(expr)
	#define assert_tcompile(expr) assert_compile(expr)
#else
	#define assert_compile(expr) typedef int __ct_assert__[1 - 2 * !(expr)]
	#define assert_tcompile(expr) assert(expr)
#endif

/* Check if the types have the bitsizes like we are using them */
assert_compile(sizeof(uint64) == 8);
assert_compile(sizeof(uint32) == 4);
assert_compile(sizeof(uint16) == 2);
assert_compile(sizeof(uint8)  == 1);

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#define M_PI   3.14159265358979323846
#endif /* M_PI_2 */

/**
 * Return the length of an fixed size array.
 * Unlike sizeof this function returns the number of elements
 * of the given type.
 *
 * @param x The pointer to the first element of the array
 * @return The number of elements
 */
#define lengthof(x) (sizeof(x) / sizeof(x[0]))

/**
 * Get the end element of an fixed size array.
 *
 * @param x The pointer to the first element of the array
 * @return The pointer past to the last element of the array
 */
#define endof(x) (&x[lengthof(x)])

/**
 * Get the last element of an fixed size array.
 *
 * @param x The pointer to the first element of the array
 * @return The pointer to the last element of the array
 */
#define lastof(x) (&x[lengthof(x) - 1])

#define cpp_offsetof(s, m)   (((size_t)&reinterpret_cast<const volatile char&>((((s*)(char*)8)->m))) - 8)
#if !defined(offsetof)
	#define offsetof(s, m) cpp_offsetof(s, m)
#endif /* offsetof */


/* take care of some name clashes on MacOS */
#if defined(__APPLE__)
	#define GetString OTTD_GetString
	#define DrawString OTTD_DrawString
	#define CloseConnection OTTD_CloseConnection
#endif /* __APPLE__ */

void NORETURN CDECL usererror(const char *str, ...) WARN_FORMAT(1, 2);
void NORETURN CDECL error(const char *str, ...) WARN_FORMAT(1, 2);
#define NOT_REACHED() error("NOT_REACHED triggered at line %i of %s", __LINE__, __FILE__)

/* For non-debug builds with assertions enabled use the special assertion handler:
 * - For MSVC: NDEBUG is set for all release builds and WITH_ASSERT overrides the disabling of asserts.
 * - For non MSVC: NDEBUG is set when assertions are disables, _DEBUG is set for non-release builds.
 */
#if (defined(_MSC_VER) && defined(NDEBUG) && defined(WITH_ASSERT)) || (!defined(_MSC_VER) && !defined(NDEBUG) && !defined(_DEBUG))
	#undef assert
	#define assert(expression) if (!(expression)) error("Assertion failed at line %i of %s: %s", __LINE__, __FILE__, #expression);
#endif

#if defined(MORPHOS) || defined(__NDS__) || defined(__DJGPP__)
	/* MorphOS and NDS don't have C++ conformant _stricmp... */
	#define _stricmp stricmp
#elif defined(OPENBSD)
	/* OpenBSD uses strcasecmp(3) */
	#define _stricmp strcasecmp
#endif

#if !defined(MAX_PATH)
	#define MAX_PATH 260
#endif

/**
 * The largest value that can be entered in a variable
 * @param type the type of the variable
 */
#define MAX_UVALUE(type) ((type)~(type)0)

#endif /* STDAFX_H */
