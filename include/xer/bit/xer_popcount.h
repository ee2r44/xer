#ifndef XER_POPCOUNT_H_
#define XER_POPCOUNT_H_

#include <cstdint>

#include "../macro/xer_macro.h"
#include "xer_bit_details.h"
#include "xer_has_single_bit.h"
#include "../env/xer_env.h"

#if XER_X86_INTRIN_AVAILABLE
#	include <immintrin.h>
#endif

#if XER_COMPILER(MSVC)&&(XER_CPU(X86_64)||XER_CPU(X86_32)||XER_CPU(ARM64))
#	include <intrin.h>
#endif

namespace xer {
	namespace details_ {

		template<typename T>
		requires bit_manipulatable<T>
		constexpr std::uint32_t popcount_fallback(T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					popcount_fallback(static_cast<std::uint64_t>(t)) +
					popcount_fallback(static_cast<std::uint64_t>(t >> 64));
			}
			else
#endif
#if XER_ARCH(32)
			if constexpr (sizeof(T) == 8) {
				return
					popcount_fallback(static_cast<std::uint32_t>(t)) +
					popcount_fallback(static_cast<std::uint32_t>(t >> 32));
			}
#endif
			{
				static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 8, "unsupported type");
				constexpr T mask0 = static_cast<T>(0x5555'5555'5555'5555);
				constexpr T mask1 = static_cast<T>(0x3333'3333'3333'3333);
				constexpr T mask2 = static_cast<T>(0x0F0F'0F0F'0F0F'0F0F);
				constexpr T multi = static_cast<T>(0x0101'0101'0101'0101);
				constexpr uint32_t needle = static_cast<uint32_t>(sizeof(T) << 3) - 8;

				t = static_cast<T>(t - ((t >> 1) & mask0));
				t = static_cast<T>((t & mask1) + ((t >> 2) & mask1));
				t = static_cast<T>((t + (t >> 4)) & mask2);

				return static_cast<std::uint32_t>(static_cast<T>(t * multi) >> needle);
			}
		}


#if (XER_X86_INTRIN_AVAILABLE&&(!XER_COMPILER(GCC))&&(!XER_COMPILER(CLANG)))||XER_POPCNT_AVAILABLE
		template<typename T>
		requires bit_manipulatable<T>
		std::uint32_t popcount_popcnt(const T t)noexcept {
#	if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					popcount_popcnt(static_cast<std::uint64_t>(t)) +
					popcount_popcnt(static_cast<std::uint64_t>(t >> 64));
			}
			else
#	endif
			if constexpr (sizeof(T) == 8) {
#	if XER_ARCH(32)
				return
					popcount_popcnt(static_cast<std::uint32_t>(t)) +
					popcount_popcnt(static_cast<std::uint32_t>(t >> 32));
#	else
				return _mm_popcnt_u64(static_cast<unsigned long long>(t));
#	endif
			}
			else {
				static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 4, "unsupported type");
				return _mm_popcnt_u32(static_cast<unsigned int>(t));
			}
		}
#endif


#if XER_COMPILER(MSVC)&&(XER_CPU(X86_64)||XER_CPU(X86_32)||XER_CPU(ARM64))
		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t popcount_msvc(const T t)noexcept {
#	if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					popcount_msvc(static_cast<std::uint64_t>(t)) +
					popcount_msvc(static_cast<std::uint64_t>(t >> 64));
			}
			else
#	endif
				if constexpr (sizeof(T) == 8) {
#	if XER_ARCH(32)
					return
						popcount_msvc(static_cast<std::uint32_t>(t)) +
						popcount_msvc(static_cast<std::uint32_t>(t >> 32));
#	else
					return __popcnt64(static_cast<unsigned long long>(t));
#	endif
				}
				else {
					static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 4, "unsupported type");
					return __popcnt(static_cast<unsigned int>(t));
				}
		}
#endif


		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t popcount_normal(const T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					popcount_normal(static_cast<std::uint64_t>(t)) +
					popcount_normal(static_cast<std::uint64_t>(t >> 64));
			}
			else
#endif
				if constexpr (sizeof(T) == 8) {
#if XER_ARCH(32)
					return
						popcount_normal(static_cast<std::uint32_t>(t)) +
						popcount_normal(static_cast<std::uint32_t>(t >> 32));
#else
#	if XER_HAS_BUILTIN(__builtin_popcountll)	
					return __builtin_popcountll(static_cast<unsigned long long>(t));
#	else
					return popcount_fallback(t);
#	endif
#endif
				}
				else {
					static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 4, "unsupported type");
#if XER_HAS_BUILTIN(__builtin_popcount)	
					return __builtin_popcount(static_cast<unsigned int>(t));
#else
					return popcount_fallback(t);
#endif
				}
		}
	}



	template<typename T>
	requires details_::bit_manipulatable<T>
	constexpr std::uint32_t popcount(const T t)noexcept {
		if consteval {
			return details_::popcount_fallback(t);
		}
		else {
#if XER_POPCNT_AVAILABLE
			return details_::popcount_popcnt(t);
#else
#	if XER_X86_INTRIN_AVAILABLE&&(!XER_COMPILER(GCC))&&(!XER_COMPILER(CLANG))
			if (is_popcnt_supported()) {
				return details_::popcount_popcnt(t);
			}
			else
#	endif
			{
#	if XER_COMPILER(MSVC)&&(XER_CPU(X86_64)||XER_CPU(X86_32)||XER_CPU(ARM64))
				return details_::popcount_msvc(t);
#	else
				return details_::popcount_normal(t);
#	endif
			}
#endif
		}
	}
}


#endif