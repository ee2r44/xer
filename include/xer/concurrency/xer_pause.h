#ifndef XER_PAUSE_H_
#define XER_PAUSE_H_

#include "../macro/xer_macro.h"

#if XER_SSE2_AVAILABLE
#	include <emmintrin.h>
#elif XER_COMPILER(MSVC)&&(XER_CPU(ARM32)||XER_CPU(ARM64)||XER_CPU(X86_64)||XER_CPU(X86_32))
#	include <intrin.h>
#endif

#if !defined(XER_PAUSE)
#	if XER_CPU(X86_64)||XER_CPU(X86_32)
#		if XER_SSE2_AVAILABLE
#			define XER_PAUSE() _mm_pause()
#		elif XER_COMPILER(MSVC)
#			define XER_PAUSE() _mm_pause()
#		else
#			define XER_PAUSE() __asm__ __volatile__("pause")
#		endif
#	elif XER_COMPILER(MSVC)&&(XER_CPU(ARM32)||XER_CPU(ARM64))
#		define XER_PAUSE() __yield()
#	elif XER_CPU(ARM32)||XER_CPU(ARM64)
#		define XER_PAUSE() __asm__ __volatile__("yield")
#	elif XER_HAS_BUILTIN(__builtin_riscv_pause)
#		define XER_PAUSE() __builtin_riscv_pause()
#	elif XER_CPU(RISCV32)||XER_CPU(RISCV64)
#		define XER_PAUSE() __asm__ __volatile__("pause")
#	elif !XER_COMPILER(MSVC)
#		define XER_PAUSE() __asm__ __volatile__("")
#	else
#		define XER_PAUSE() ((void)0)
#	endif
#endif

#endif