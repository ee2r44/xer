#ifndef XER_MACRO_H_
#define XER_MACRO_H_

//XER_CPLUSPLUS BEGIN
#if (!defined(XER_CPLUSPLUS))&&defined(__cplusplus)
#	if defined(_MSVC_LANG)
#		define XER_CPLUSPLUS _MSVC_LANG
#	else
#		define XER_CPLUSPLUS __cplusplus
#	endif
#endif

#if defined(XER_CPLUSPLUS)
static_assert(XER_CPLUSPLUS >= 202302L, "c++ version must at least be c++23");
#endif
//XER_CPLUSPLUS END





//XER_ARCH BEGIN
#define XER_ARCH_UNKNOWN 0
#define XER_ARCH_32 1
#define XER_ARCH_64 2

#if !defined(XER_ARCH_THIS)
#  if defined(__SIZEOF_POINTER__)
#    if __SIZEOF_POINTER__ == 8
#      define XER_ARCH_THIS XER_ARCH_64
#    elif __SIZEOF_POINTER__ == 4
#      define XER_ARCH_THIS XER_ARCH_32
#    else
#      define XER_ARCH_THIS XER_ARCH_UNKNOWN
#    endif
#  elif defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64)
#    define XER_ARCH_THIS XER_ARCH_64
#  elif defined(_M_IX86) || defined(_M_ARM)
#    define XER_ARCH_THIS XER_ARCH_32
#  elif defined(__LP64__) || defined(_LP64)
#    define XER_ARCH_THIS XER_ARCH_64
#  elif defined(__ILP32__) || defined(_ILP32)
#    define XER_ARCH_THIS XER_ARCH_32
#  else
#    define XER_ARCH_THIS XER_ARCH_UNKNOWN
#  endif
#endif


#define XER_ARCH(x) (XER_ARCH_##x == XER_ARCH_THIS)


static_assert(XER_ARCH(32) || XER_ARCH(64), "unsupported architecture");
//XER_ARCH END


//XER_PLATFORM BEGIN
#define XER_PLATFORM_UNKNOWN 0
#define XER_PLATFORM_WINDOWS 1
#define	XER_PLATFORM_POSIX 2

#if !defined(XER_PLATFORM_THIS)
#	if defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__MINGW64__)
#		if defined(__CYGWIN__) || defined(__MSYS__)
#			define XER_PLATFORM_THIS XER_PLATFORM_POSIX
#		else
#			define XER_PLATFORM_THIS XER_PLATFORM_WINDOWS
#		endif
#	elif defined(__unix__) || defined(__unix) \
			 || (defined(__APPLE__) && defined(__MACH__)) \
			 || defined(__linux__)
#		define XER_PLATFORM_THIS XER_PLATFORM_POSIX
#	else
#		define XER_PLATFORM_THIS XER_PLATFORM_UNKNOWN
#	endif
#endif

#define XER_PLATFORM(x) (XER_PLATFORM_##x == XER_PLATFORM_THIS)
//XER_PLATFORM END

//XER_COMPILER BEGIN
#define XER_COMPILER_UNKNOWN 0
#define XER_COMPILER_MSVC 1
#define XER_COMPILER_CLANG 2
#define XER_COMPILER_GCC 3

#if !defined(XER_COMPILER_THIS)
#	if defined(__clang__)
#		define XER_COMPILER_THIS XER_COMPILER_CLANG
#	elif defined(_MSC_VER)
#		define XER_COMPILER_THIS XER_COMPILER_MSVC
#	elif defined(__GNUC__)
#		define XER_COMPILER_THIS XER_COMPILER_GCC
#	else
#		define XER_COMPILER_THIS XER_COMPILER_UNKNOWN
#	endif
#endif

#define XER_COMPILER(x) (XER_COMPILER_##x == XER_COMPILER_THIS)
//XER_COMPILER END

//XER_CPU BEGIN
#define XER_CPU_UNKNOWN 0
#define XER_CPU_X86_64 1
#define XER_CPU_X86_32 2
#define XER_CPU_ARM64 3
#define XER_CPU_ARM32 4
#define XER_CPU_RISCV64 5
#define XER_CPU_RISCV32 6

#if !defined(XER_CPU_THIS)
#	if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
#		define XER_CPU_THIS XER_CPU_X86_64
#	elif defined(_M_IX86) || defined(__i386__) || defined(__i686__)
#		define XER_CPU_THIS XER_CPU_X86_32
#	elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
#		define XER_CPU_THIS XER_CPU_ARM64
#	elif defined(_M_ARM) || defined(__arm__)
#		define XER_CPU_THIS XER_CPU_ARM32
#	elif defined(__riscv)
#		if __riscv_xlen == 64
#			define XER_CPU_THIS XER_CPU_RISCV64
#		else
#			define XER_CPU_THIS XER_CPU_RISCV32
#		endif
#	else
#		define XER_CPU_THIS XER_CPU_UNKNOWN
#	endif
#endif

#define XER_CPU(x) (XER_CPU_##x == XER_CPU_THIS)
//XER_CPU END


//XER_BYTE_ORDER BEGIN
#define XER_BYTE_ORDER_LITTLE 0
#define XER_BYTE_ORDER_BIG 1

#if !defined(XER_BYTE_ORDER_THIS)
#	if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) \
		&& __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#		define XER_BYTE_ORDER_THIS XER_BYTE_ORDER_LITTLE
#	elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) \
		&& __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#		define XER_BYTE_ORDER_THIS XER_BYTE_ORDER_BIG
#	elif defined(_MSC_VER)
#		define XER_BYTE_ORDER_THIS XER_BYTE_ORDER_LITTLE
#	else
#		error "Cannot determine endianness"
#	endif
#endif

#define XER_BYTE_ORDER(x) (XER_BYTE_ORDER_##x == XER_BYTE_ORDER_THIS)

static_assert(XER_BYTE_ORDER(BIG) || XER_BYTE_ORDER(LITTLE), "unsupported byte order");
//XER_BYTE_ORDER END

//XER_HAS_CPP_ATTRIBUTE BEGIN
#if defined(__has_cpp_attribute)
#	define XER_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#	define XER_HAS_CPP_ATTRIBUTE(x) false
#endif
//XER_HAS_CPP_ATTRIBUTE END

//XER_HAS_BUILTIN BEGIN
#if defined(__has_builtin)
#	define XER_HAS_BUILTIN(x) __has_builtin(x)
#else
#	define XER_HAS_BUILTIN(x) false
#endif
/*
* __builtin_uadd_overflow
* __builtin_uaddll_overflow
* __builtin_usub_overflow
* __builtin_usubll_overflow
* __builtin_ctz_overflow
* __builtin_ctzll_overflow
* __builtin_ctzg_overflow
*/
//XER_HAS_BUILTIN END


//XER_HAS_BUILTIN_ADDRESSOF BEGIN
#if !defined(XER_HAS_BUILTIN_ADDRESSOF)
#	define XER_HAS_BUILTIN_ADDRESSOF (XER_HAS_BUILTIN(__builtin_addressof)||XER_COMPILER(MSVC))
#endif
//XER_HAS_BUILTIN_ADDRESSOF END

//XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE BEGIN
#if !defined(XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE)
#	define XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE (XER_COMPILER(MSVC)||XER_HAS_BUILTIN(__integer_pack)||XER_HAS_BUILTIN(__make_integer_seq))
#endif
//XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE END

//XER_HAS_BUILTIN_BIT_CAST BEGIN
#if !defined(XER_HAS_BUILTIN_BIT_CAST)
#	if XER_COMPILER(MSVC)
#		define XER_HAS_BUILTIN_BIT_CAST (_MSC_VER>=1926)
#	else
#		define XER_HAS_BUILTIN_BIT_CAST XER_HAS_BUILTIN(__builtin_bit_cast)
#	endif
#endif
//XER_HAS_BUILTIN_BIT_CAST END


//XER_HAS_MSVC_WIDE_MUL BEGIN
#if !defined(XER_HAS_MSVC_WIDE_MUL)
#	define XER_HAS_MSVC_WIDE_MUL (XER_COMPILER(MSVC) && XER_ARCH(64)&&(XER_CPU(X86_64) || XER_CPU(ARM64)))
#endif
//XER_HAS_MSVC_WIDE_MUL END


//XER_X86_INTRIN_AVAILABLE BEGIN
#if !defined(XER_X86_INTRIN_AVAILABLE)
#	define XER_X86_INTRIN_AVAILABLE ((XER_CPU(X86_64)||XER_CPU(X86_32))&&\
									(XER_COMPILER(MSVC)||XER_COMPILER(GCC)||XER_COMPILER(CLANG)))
#endif
//XER_X86_INTRIN_AVAILABLE END

//XER_HAS_BUILTIN_INT128 BEGIN
#if !defined(XER_HAS_BUILTIN_INT128)
#	if defined(__SIZEOF_INT128__)
#		define XER_HAS_BUILTIN_INT128 true
#	else
#		define XER_HAS_BUILTIN_INT128 false
#	endif
#endif
//XER_HAS_BUILTIN_INT128 END

//XER_SSE2_AVAILABLE BEGIN
#if !defined(XER_SSE2_AVAILABLE)
#	if defined(__SSE2__)||(XER_CPU(X86_64))
#		define XER_SSE2_AVAILABLE true
#	else
#		define XER_SSE2_AVAILABLE false
#	endif
#endif
//XER_SSE2_AVAILABLE END


//XER_BMI1_AVAILABLE BEGIN
#if !defined(XER_BMI1_AVAILABLE)
#	if defined(__BMI__) || (XER_COMPILER(MSVC) && _MSC_VER >= 1900 && defined(__AVX2__))
#		define XER_BMI1_AVAILABLE true
#	else
#		define XER_BMI1_AVAILABLE false
#	endif
#endif
//XER_BMI1_AVAILABLE END

//XER_POPCNT_AVAILABLE BEGIN
#if !defined(XER_POPCNT_AVAILABLE)
#	if defined(__POPCNT__) ||(XER_COMPILER(MSVC) && defined(__AVX__))
#		define XER_POPCNT_AVAILABLE true
#	else
#		define XER_POPCNT_AVAILABLE false
#	endif
#endif
//XER_POPCNT_AVAILABLE END


//XER_HAS_ALIGNED_ALLOC BEGIN
#if !defined(XER_HAS_ALIGNED_ALLOC)
#	if XER_PLATFORM(WINDOWS)
#		define XER_HAS_ALIGNED_ALLOC false
#	elif defined(__GLIBC__)
#		define XER_HAS_ALIGNED_ALLOC (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 16))
#	else
#		define XER_HAS_ALIGNED_ALLOC false
#	endif
#endif
//XER_HAS_ALIGNED_ALLOC END

//XER_RESTRICT BEGIN
#if !defined(XER_RESTRICT)
#	if !defined(__cplusplus)
#		define XER_RESTRICT restrict
#	elif defined(__INTEL_COMPILER) || defined(__ICC) || defined(__ICL)
#		define XER_RESTRICT __restrict__
#	elif defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#		define XER_RESTRICT __restrict__
#	elif defined(_MSC_VER)
#		define XER_RESTRICT __restrict
#	else
#		define XER_RESTRICT
#	endif
#endif
//XER_RESTRICT END


//XER_*LIKELY BEGIN
/*
* use case:
* if(...)XER_LIKELY{
* ...
* }
*/
#if defined(__cplusplus)
#	if !defined(XER_LIKELY)
#		if XER_HAS_CPP_ATTRIBUTE(likely)
#			define XER_LIKELY [[likely]]
#		elif XER_CPLUSPLUS >= 202002L
#				define XER_LIKELY [[likely]]
#		else
#				define XER_LIKELY
#		endif
#	endif
#	if !defined(XER_UNLIKELY)
#		if XER_HAS_CPP_ATTRIBUTE(unlikely)
#			define XER_UNLIKELY [[unlikely]]
#		elif XER_CPLUSPLUS >= 202002L
#				define XER_UNLIKELY [[unlikely]]
#		else
#				define XER_UNLIKELY
#		endif
#	endif
#endif
//XER_*LIKELY END

//XER_NOINLINE BEGIN
#if !defined(XER_NOINLINE)
#	if XER_COMPILER(MSVC)
#		define XER_NOINLINE __declspec(noinline)
#	elif XER_COMPILER(GCC) || XER_COMPILER(CLANG)
#		define XER_NOINLINE __attribute__((noinline))
#	else
#		define XER_NOINLINE
#	endif
#endif
//XER_NOINLINE END

//XER_FORCE_INLINE BEGIN
#if !defined(XER_FORCE_INLINE)
#	if XER_COMPILER(MSVC)
#		define XER_FORCE_INLINE __forceinline
#	elif XER_COMPILER(GCC)||XER_COMPILER(CLANG)
#		define XER_FORCE_INLINE __attribute__((always_inline)) inline
#	else
#		define XER_FORCE_INLINE
#	endif
#endif
//XER_FORCE_INLINE END



// XER_CACHE_LINE_SIZE BEGIN
#if !defined(XER_CACHE_LINE_SIZE)
#	if defined(__GCC_DESTRUCTIVE_SIZE)
#		define XER_CACHE_LINE_SIZE __GCC_DESTRUCTIVE_SIZE
#	elif defined(__APPLE__) && XER_CPU(ARM64)
#		define XER_CACHE_LINE_SIZE 128
#	else
#		define XER_CACHE_LINE_SIZE 64
#	endif
#endif
static_assert(
	XER_CACHE_LINE_SIZE > 0 &&
	(XER_CACHE_LINE_SIZE & (XER_CACHE_LINE_SIZE - 1)) == 0,
	"XER_CACHE_LINE_SIZE must be a power of two");
// XER_CACHE_LINE_SIZE END

// XER_PREFETCH BEGIN
#if !defined(XER_PREFETCH)
#	if XER_COMPILER(GCC) || XER_COMPILER(CLANG)
#		define XER_PREFETCH(p) __builtin_prefetch(static_cast<const void*>(p), 0, 3)
#	elif XER_X86_INTRIN_AVAILABLE
#		include <immintrin.h>  // 若该头尚未间接包含
#		define XER_PREFETCH(p) _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_T0)
#	else
#		define XER_PREFETCH(p) ((void)(p))
#	endif
#endif
// 预取从 p 起大约覆盖 n 个 T 的只读数据（按 cache line 步进）
#if !defined(XER_PREFETCH_N)
#	define XER_PREFETCH_N(p, n)                                                      \
		do {                                                                         \
			const char* xer_pf0 = reinterpret_cast<const char*>(p);                  \
			const char* xer_pf1 =                                                    \
				xer_pf0 + sizeof(*(p)) * static_cast<std::size_t>(n);                \
			for (; xer_pf0 < xer_pf1; xer_pf0 += XER_CACHE_LINE_SIZE) {              \
				XER_PREFETCH(xer_pf0);                                               \
			}                                                                        \
		} while (0)
#endif
// XER_PREFETCH END

#endif