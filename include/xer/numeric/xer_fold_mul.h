#ifndef XER_FOLD_MUL_H_
#define XER_FOLD_MUL_H_

#include <cstdint>

#include "../macro/xer_macro.h"

#if XER_HAS_MSVC_WIDE_MUL&&(XER_CPU(X86_64)||XER_CPU(ARM64))
#	include <intrin.h>
#endif

namespace xer {
	namespace details_ {
		inline constexpr std::uint64_t fold_mulu64_fallback(const std::uint64_t a, const std::uint64_t b)noexcept {
			const std::uint32_t a0 = static_cast<std::uint32_t>(a);
			const std::uint32_t a1 = static_cast<std::uint32_t>(a>>32);

			const std::uint32_t b0 = static_cast<std::uint32_t>(b);
			const std::uint32_t b1 = static_cast<std::uint32_t>(b >> 32);

			const std::uint64_t a0b0 = static_cast<std::uint64_t>(a0) * b0;
			const std::uint64_t a0b1 = static_cast<std::uint64_t>(a0) * b1;
			const std::uint64_t a1b0 = static_cast<std::uint64_t>(a1) * b0;
			const std::uint64_t a1b1 = static_cast<std::uint64_t>(a1) * b1;

			const std::uint64_t middle =
				static_cast<std::uint64_t>(static_cast<std::uint32_t>(a0b1)) +
				static_cast<std::uint32_t>(a1b0)+
				(a0b0>>32);

			return
				((middle << 32) | static_cast<std::uint32_t>(a0b0)) ^
				(a1b1 + (a0b1 >> 32) + (a1b0 >> 32) + (middle >> 32));

		}
	}

	inline constexpr std::uint64_t fold_mulu64(const std::uint64_t a, const std::uint64_t b)noexcept {
		if consteval {
			return details_::fold_mulu64_fallback(a, b);
		}
		else {
#if XER_HAS_MSVC_WIDE_MUL&&XER_CPU(X86_64)
			unsigned long long high;
			const std::uint64_t low = _umul128(a, b, &high);
			return low ^ high;
#elif XER_HAS_MSVC_WIDE_MUL&&XER_CPU(ARM64)
			return (a * b) ^ __umulh(a, b);
#elif XER_HAS_BUILTIN_INT128
			const unsigned __int128 r = static_cast<unsigned __int128>(a) * b;
			return
				static_cast<std::uint64_t>(r >> 64) ^
				static_cast<std::uint64_t>(r);
#else
			return details_::fold_mulu64_fallback(a, b);
#endif
		}

	}
}

#endif