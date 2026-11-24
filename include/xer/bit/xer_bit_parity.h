#ifndef XER_BIT_PARITY_H_
#define XER_BIT_PARITY_H_

#include <cstdint>

#include "xer_has_single_bit.h"
#include "xer_bit_details.h"
#include "xer_popcount.h"
#include "../env/xer_env.h"
#include "../macro/xer_macro.h"

namespace xer {
	namespace details_ {
		template<typename T>
		requires bit_manipulatable<T>
		constexpr std::uint32_t bit_parity_fallback(T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					bit_parity_fallback(static_cast<std::uint64_t>(t)) ^
					bit_parity_fallback(static_cast<std::uint64_t>(t >> 64));
			}
			else
#endif
#if XER_ARCH(32)
			if constexpr (sizeof(T) == 8) {
				return
					bit_parity_fallback(static_cast<std::uint32_t>(t)) ^
					bit_parity_fallback(static_cast<std::uint32_t>(t >> 32));
			}
			else
#endif
			{
				static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 8, "unsupported type");

				if constexpr (sizeof(T) >= 8) t ^= static_cast<T>(t >> 32);
				if constexpr (sizeof(T) >= 4) t ^= static_cast<T>(t >> 16);
				if constexpr (sizeof(T) >= 2) t ^= static_cast<T>(t >> 8);
				t ^= static_cast<T>(t >> 4);
				t ^= static_cast<T>(t >> 2);
				t ^= static_cast<T>(t >> 1);

				return static_cast<std::uint32_t >(t & T{ 1 });
			}
		}



		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t bit_parity_normal(const T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				return
					bit_parity_normal(static_cast<std::uint64_t>(t)) ^
					bit_parity_normal(static_cast<std::uint64_t>(t >> 64));
			}
			else
#endif
			if constexpr (sizeof(T) == 8) {
#if XER_ARCH(32)
				return
					bit_parity_normal(static_cast<std::uint32_t>(t)) ^
					bit_parity_normal(static_cast<std::uint32_t>(t >> 32));
#else
#	if XER_HAS_BUILTIN(__builtin_parityll)
				return
					__builtin_parityll(static_cast<unsigned long long>(t));
#	else
				return
					bit_parity_fallback(t);
#	endif
#endif
			}
			else{
				static_assert(has_single_bit(sizeof(T)) && sizeof(T) <= 4, "unsupported type");
#if XER_HAS_BUILTIN(__builtin_parity)
				return
					__builtin_parity(static_cast<unsigned int>(t));
#else
				return
					bit_parity_fallback(t);
#endif
			}
		}
	}


	template<typename T>
	requires details_::bit_manipulatable<T>
	constexpr std::uint32_t bit_parity(const T t)noexcept {
		if consteval {
			return details_::bit_parity_fallback(t);
		}
		else {
#if XER_POPCNT_AVAILABLE
			return details_::popcount_popcnt(t) & 1;
#else
#	if XER_COMPILER(MSVC)&&(XER_CPU(X86_64)||XER_CPU(X86_32)||XER_CPU(ARM64))
#		if XER_X86_INTRIN_AVAILABLE
			if (is_popcnt_supported()) {
				return details_::popcount_popcnt(t) & 1;
			}
			else
#		endif
			{
				return details_::popcount_msvc(t) & 1;
			}
#	else
			return details_::bit_parity_normal(t);
#	endif
#endif
		}
	}

}

#endif