#ifndef XER_BYTESWAP_H_
#define XER_BYTESWAP_H_

#include <cstdint>

#include "../macro/xer_macro.h"
#include "xer_bit_details.h"

#if XER_COMPILER(MSVC)
#	include <stdlib.h>
#endif


static_assert(XER_ARCH(32) || XER_ARCH(64), "unsupported architecture");

namespace xer {
	namespace details_ {
		template<typename T>
		requires
		bit_manipulatable<T>
		constexpr T byteswap_fallback(T t)noexcept {


			if constexpr (sizeof(T) == 1) {
				return static_cast<T>(t);
			}
			else if constexpr (sizeof(T) == 2) {
				return static_cast<T>((t >> 8) | (t << 8));
			}
			else if constexpr (sizeof(T) == 4) {
				constexpr T mask = 0x00ff'00ff;
				t = ((t & mask) << 8) | ((t >> 8) & mask);
				return static_cast<T>((t >> 16) | (t << 16));
			}
			else if constexpr (sizeof(T) == 8) {
#if XER_ARCH(64)
				constexpr T mask0 = 0x00ff'00ff'00ff'00ff;
				constexpr T mask1 = 0x0000'ffff'0000'ffff;

				t = ((t & mask0) << 8) | ((t >> 8) & mask0);
				t = ((t & mask1) << 16) | ((t >> 16) & mask1);

				return static_cast<T>((t >> 32) | (t << 32));
#elif XER_ARCH(32)
				return
					(static_cast<T>(byteswap_fallback(static_cast<std::uint32_t>(t))) << 32) |
					byteswap_fallback(static_cast<std::uint32_t>(t >> 32));
#endif
			}
#if		XER_HAS_BUILTIN_INT128
			else if constexpr (sizeof(T) == 16) {
				return
					(static_cast<T>(byteswap_fallback(static_cast<std::uint64_t>(t))) << 64) |
					byteswap_fallback(static_cast<std::uint64_t>(t >> 64));
			}
#endif
			else {
				static_assert(false, "unsupported unsigned-integer type");
			}

		}

		constexpr std::uint16_t byteswap(const std::uint16_t t)noexcept {
			if consteval {
				return details_::byteswap_fallback(t);
			}
			else {
#if XER_COMPILER(MSVC)
				static_assert(sizeof(unsigned short) == 2, "byteswap error");
				return _byteswap_ushort(t);
#elif XER_HAS_BUILTIN(__builtin_bswap16)
				return __builtin_bswap16(t);
#else
				return details_::byteswap_fallback(t);
#endif
			}
		}

		constexpr std::uint32_t byteswap(const std::uint32_t t)noexcept {
			if consteval {
				return details_::byteswap_fallback(t);
			}
			else {
#if XER_COMPILER(MSVC)
				static_assert(sizeof(unsigned long) == 4, "byteswap error");
				return _byteswap_ulong(t);
#elif XER_HAS_BUILTIN(__builtin_bswap32)
				return __builtin_bswap32(t);
#else
				return details_::byteswap_fallback(t);
#endif
			}
		}


		constexpr std::uint64_t byteswap(const std::uint64_t t)noexcept {
			if consteval {
				return details_::byteswap_fallback(t);
			}
			else {
#if XER_ARCH(32)
				return
					(static_cast<std::uint64_t>(byteswap(static_cast<std::uint32_t>(t))) << 32) |
					byteswap(static_cast<std::uint32_t>(t >> 32));
#elif XER_COMPILER(MSVC)
				return _byteswap_uint64(t);
#elif XER_HAS_BUILTIN(__builtin_bswap64)
				return __builtin_bswap64(t);
#else
				return details_::byteswap_fallback(t);
#endif
			}
		}


#if XER_HAS_BUILTIN_INT128
		constexpr unsigned __int128 byteswap(const unsigned __int128 t)noexcept {
			if consteval {
				return details_::byteswap_fallback(t);
			}
			else {
				return
					(static_cast<unsigned __int128>(byteswap(static_cast<std::uint64_t>(t))) << 64) |
					byteswap(static_cast<std::uint64_t>(t >> 64));
			}
		}
#endif
	}


	template<typename T>
	requires details_::bit_manipulatable<T>
	constexpr T byteswap(const T t)noexcept {
		if constexpr (sizeof(T) == 1) {
			return static_cast<T>(t);
		}
		else if constexpr (sizeof(T) == 2) {
			return details_::byteswap(static_cast<std::uint16_t>(t));
		}
		else if constexpr (sizeof(T) == 4) {
			return details_::byteswap(static_cast<std::uint32_t>(t));
		}
		else if constexpr (sizeof(T) == 8) {
			return details_::byteswap(static_cast<std::uint64_t>(t));
		}
#if XER_HAS_BUILTIN_INT128
		else if constexpr (sizeof(T) == 16) {
			return details_::byteswap(static_cast<unsigned __int128>(t));
		}
#endif
		else {
			static_assert(false, "unsupported type");
			return T{};
		}
	}
}

#endif