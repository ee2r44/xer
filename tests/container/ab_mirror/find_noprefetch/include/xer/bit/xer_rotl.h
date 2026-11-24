#ifndef XER_ROTL_H_
#define XER_ROTL_H_

#include <cstdint>

#include "../macro/xer_macro.h"
#include "xer_bit_details.h"

#if XER_COMPILER(MSVC)
#	if XER_X86_INTRIN_AVAILABLE
#		include <intrin.h>
#pragma intrinsic(_rotl8)
#pragma intrinsic(_rotl16)
#	else
#		include <stdlib.h>
#	endif
#pragma intrinsic(_rotl)
#pragma intrinsic(_rotl64)
#elif XER_X86_INTRIN_AVAILABLE&&(XER_COMPILER(GCC)||XER_COMPILER(CLANG))
#	include <x86intrin.h>
#endif


namespace xer {
	namespace details_ {

		template<typename T>
			requires bit_manipulatable<T>
		constexpr T rotl_fallback(const T t,const std::uint32_t count)noexcept {
			constexpr std::uint32_t bit_count = sizeof(T) << 3;
			constexpr std::uint32_t mask = bit_count - 1;
			const std::uint32_t c = count & mask;
			return
				(t << c) |
				(t >> ((static_cast<std::uint32_t>(0) - c) & mask));
		}

	}

	template<typename T>
	requires details_::bit_manipulatable<T>
	constexpr T rotl(const T t, const std::uint32_t count)noexcept {
		if consteval {
			return details_::rotl_fallback(t, count);
		}
		else {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return details_::rotl_fallback(t, count);
			}
			else
#endif
			if constexpr (sizeof(T) == 8) {
#if XER_COMPILER(MSVC)
				return _rotl64(t, count);
#elif XER_HAS_BUILTIN(__builtin_rotateleft64)
				return __builtin_rotateleft64(t, count);
#elif XER_CPU(X86_64)&&XER_X86_INTRIN_AVAILABLE&&(XER_COMPILER(GCC)||XER_COMPILER(CLANG))
				return __rolq(t, count);
#else
				return details_::rotl_fallback(t, count);
#endif
			}
			else if constexpr (sizeof(T) == 4) {
#if XER_COMPILER(MSVC)
				return _rotl(t, count);
#elif XER_HAS_BUILTIN(__builtin_rotateleft32)
				return __builtin_rotateleft32(t, count);
#elif XER_X86_INTRIN_AVAILABLE&&(XER_COMPILER(GCC)||XER_COMPILER(CLANG))
				return __rold(t, count);
#else
				return details_::rotl_fallback(t, count);
#endif
			}
			else if constexpr (sizeof(T) == 2) {
#if XER_X86_INTRIN_AVAILABLE&&XER_COMPILER(MSVC)
				return _rotl16(t, count);
#elif XER_HAS_BUILTIN(__builtin_rotateleft16)
				return __builtin_rotateleft16(t, count);
#elif XER_X86_INTRIN_AVAILABLE&&(XER_COMPILER(GCC)||XER_COMPILER(CLANG))
				return __rolw(t, count);
#else
				return details_::rotl_fallback(t, count);
#endif
			}
			else if constexpr (sizeof(T) == 1) {
#if XER_X86_INTRIN_AVAILABLE&&XER_COMPILER(MSVC)
				return _rotl8(t, count);
#elif XER_HAS_BUILTIN(__builtin_rotateleft8)
				return __builtin_rotateleft8(t, count);
#elif XER_X86_INTRIN_AVAILABLE&&(XER_COMPILER(GCC)||XER_COMPILER(CLANG))
				return __rolb(t, count);
#else
				return details_::rotl_fallback(t, count);
#endif
			}
			else {
				static_assert(false, "unsupported type");
				return T{};
			}
		}
	}
}

#endif