#ifndef XER_COUNTR_ZERO_H_
#define XER_COUNTR_ZERO_H_

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "xer_bit_details.h"
#include "../macro/xer_macro.h"
#include "../container/xer_array.h"
#include "../env/xer_env.h"

#if XER_COMPILER(MSVC)
//x86/x64:tzcnt,_BitScanForward
//ARM32/ARM64:_BitScanForward
#  include <intrin.h>
#elif XER_X86_INTRIN_AVAILABLE
//x86/x64:tzcnt
#include <immintrin.h>
#endif

namespace xer {
	namespace details_ {
		template<typename T>
		requires
		bit_manipulatable<T>
		consteval array<uint8_t, sizeof(T) << 3>countr_zero_de_bruijn_table()noexcept {
			if constexpr (sizeof(T) == 1) {
				return array<uint8_t, sizeof(T) << 3>
				{ 0, 1, 6, 2, 7, 5, 4, 3 };
			}
			else if constexpr (sizeof(T) == 2) {
				return array<uint8_t, sizeof(T) << 3>
				{0, 1, 8, 2, 14, 9, 11, 3, 15, 7, 13, 10, 6, 12, 5, 4};
			}
			else if constexpr (sizeof(T) == 4) {
				return array<uint8_t, sizeof(T) << 3>
				{0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
					31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
			}
			else if constexpr (sizeof(T) == 8) {
				return array<uint8_t, sizeof(T) << 3>
				{0, 1, 48, 2, 57, 49, 28, 3, 61, 58, 50, 42, 38, 29, 17, 4,
					62, 55, 59, 36, 53, 51, 43, 22, 45, 39, 33, 30, 24, 18, 12, 5,
					63, 47, 56, 27, 60, 41, 37, 16, 54, 35, 52, 21, 44, 32, 23, 11,
					46, 26, 40, 15, 34, 20, 31, 10, 25, 14, 19, 9, 13, 8, 7, 6};
			}
			else {
				static_assert(false, "invalid type");
			}

		}



		template<typename T>
		requires bit_manipulatable<T>
		constexpr std::uint32_t countr_zero_fallback(const T t)noexcept {
			if (t == 0) {
				constexpr std::uint32_t bit_count = sizeof(T) << 3;
				return bit_count;
			}
			else {
#if XER_HAS_BUILTIN_INT128
				if constexpr (sizeof(T) == 16) {
					const std::uint64_t low = static_cast<std::uint64_t>(t);
					const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);

					return low == 0 ?
						(64 + countr_zero_fallback(high)) :
						countr_zero_fallback(low);
				}
				else
#endif
#if XER_ARCH(32)
					if constexpr (sizeof(T) == 8) {
						const std::uint32_t low = static_cast<std::uint32_t>(t);
						const std::uint32_t high = static_cast<std::uint32_t>(t >> 32);

						return low == 0 ?
							(32 + countr_zero_fallback(high)) :
							countr_zero_fallback(low);
					}
					else
#endif
					{
						static constexpr array<uint8_t, sizeof(T) << 3>table = countr_zero_de_bruijn_table<T>();

						constexpr int needle0 = [] {
							if constexpr (sizeof(T) == 1) return 3;
							else if constexpr (sizeof(T) == 2) return 4;
							else if constexpr (sizeof(T) == 4) return 5;
							else if constexpr (sizeof(T) == 8) return 6;
							else {
								static_assert(false, "invalid type");
								return 0;
							}
							}();
						constexpr int needle1 = static_cast<int>((sizeof(T) << 3) - needle0);

						constexpr T B = [] {
							if constexpr (sizeof(T) == 1) return static_cast<T>(0x1D);
							else if constexpr (sizeof(T) == 2) return static_cast<T>(0x0F2D);
							else if constexpr (sizeof(T) == 4) return static_cast<T>(0x077C'B531);
							else if constexpr (sizeof(T) == 8) return static_cast<T>(0x03F7'9D71'B4CB'0A89);
							else {
								static_assert(false, "invalid type");
								return T{};
							}
							}();

						const std::size_t index = static_cast<std::size_t>(
							static_cast<T>(static_cast<T>(t & static_cast<T>(T{ 0 } - t)) * B) >> needle1);
						return static_cast<std::uint32_t>(table[index]);
					}
			}
		}




#if XER_BMI1_AVAILABLE
		static_assert(XER_CPU(X86_64) || XER_CPU(X86_32), "only x86/64 cpu support bmi1");
		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t countr_zero_bmi1(const T t)noexcept {
#	if (XER_COMPILER(GCC) || XER_COMPILER(CLANG)) && XER_HAS_BUILTIN(__builtin_ctzg)
				return __builtin_ctzg(t, static_cast<int>(sizeof(T) << 3));
#	else
#		if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				const std::uint64_t low = static_cast<std::uint64_t>(t);
				const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);
				return
					low == 0 ?
					static_cast<std::uint32_t>(64) + _tzcnt_u64(high) :
					_tzcnt_u64(low);
			}
			else
#		endif
			if constexpr (sizeof(T) == 8) {
#		if XER_ARCH(64)
				return _tzcnt_u64(t);
#		elif XER_ARCH(32)
				const std::uint32_t low = static_cast<std::uint32_t>(t);
				const std::uint32_t high = static_cast<std::uint32_t>(t >> 32);
				return
					low == 0 ?
					static_cast<uint32_t>(32) + _tzcnt_u32(high) :
					_tzcnt_u32(low);
#		endif
			}
			else if constexpr (sizeof(T) == 4) {
				return _tzcnt_u32(t);
			}
			else if constexpr (sizeof(T) == 2) {
				constexpr std::uint32_t mask = 0xFFFF'0000;
				return _tzcnt_u32(t | mask);
			}
			else if constexpr (sizeof(T) == 1) {
				constexpr std::uint32_t mask = 0xFFFF'FF00;
				return _tzcnt_u32(t | mask);
			}
			else {
				static_assert(false, "unsupported type");
				return 0;
			}
#	endif
	}
#endif

//GCC 下且无-mbmi时调用含有tzcnt的函数需要__attribute__((target("bmi")))，导致无法内联
#if (!XER_COMPILER(GCC))&&XER_X86_INTRIN_AVAILABLE
		template<typename T>
		requires bit_manipulatable<T>
		std::uint32_t countr_zero_tzcnt(const T t)noexcept {
#	if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				const std::uint64_t low = static_cast<std::uint64_t>(t);
				const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);
				return
					low == 0 ?
					static_cast<std::uint32_t>(64) + _tzcnt_u64(high) :
					_tzcnt_u64(low);
			}
			else
#	endif
			if constexpr (sizeof(T) == 8) {
#	if XER_ARCH(64)
				return _tzcnt_u64(t);
#	elif XER_ARCH(32)
				const std::uint32_t low = static_cast<std::uint32_t>(t);
				const std::uint32_t high = static_cast<std::uint32_t>(t >> 32);
				return
					low == 0 ?
					32 + _tzcnt_u32(high) :
					_tzcnt_u32(low);
#	endif
			}
			else if constexpr (sizeof(T) == 4) {
				return _tzcnt_u32(t);
			}
			else if constexpr (sizeof(T) == 2) {
				constexpr std::uint32_t mask = 0xFFFF0000;
				return _tzcnt_u32(t | mask);
			}
			else if constexpr (sizeof(T) == 1) {
				constexpr std::uint32_t mask = 0xFFFFFF00;
				return _tzcnt_u32(t | mask);
			}
			else {
				static_assert(false, "unsupported type");
				return 0;
			}
		}
#endif


#if XER_COMPILER(GCC)
		template<typename T>
		requires bit_manipulatable<T>
		std::uint32_t countr_zero_gcc(const T t)noexcept {

#	if XER_HAS_BUILTIN(__builtin_ctzg)
			return __builtin_ctzg(t, static_cast<int>(sizeof(T) << 3));
#	else
#		if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				const std::uint64_t low = static_cast<std::uint64_t>(t);
				const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);

				return
					low == 0 ?
					64 + countr_zero_gcc(high) :
					countr_zero_gcc(low);
			}
			else
#		endif
			if constexpr (sizeof(T) == 8) {
#		if XER_ARCH(64)
#			if XER_HAS_BUILTIN(__builtin_ctzll)
				return t == 0 ? 64 : __builtin_ctzll(t);
#			else
				return countr_zero_fallback(t);
#			endif
#		elif XER_ARCH(32)
				const std::uint32_t low = static_cast<std::uint32_t>(t);
				const std::uint32_t high = static_cast<std::uint32_t>(t>>32);

				return
					low == 0 ?
					32 + countr_zero_gcc(high) :
					countr_zero_gcc(low);
#		endif
			}
			else if constexpr (sizeof(T) == 4) {
#		if XER_HAS_BUILTIN(__builtin_ctz)
				return t == 0 ? 32 : __builtin_ctz(t);
#		else
				return countr_zero_fallback(t);
#		endif
			}
			else if constexpr (sizeof(T) == 2) {
#		if XER_HAS_BUILTIN(__builtin_ctz)
				constexpr std::uint32_t mask = 0xFFFF0000;
				return t == 0 ? 16 : __builtin_ctz(t|mask);
#		else
				return countr_zero_fallback(t);
#		endif
			}
			else if constexpr (sizeof(T) == 1) {
#		if XER_HAS_BUILTIN(__builtin_ctz)
				constexpr std::uint32_t mask = 0xFFFFFF00;
				return t == 0 ? 8 : __builtin_ctz(t | mask);
#		else
				return countr_zero_fallback(t);
#		endif
			}
			else {
				static_assert(false, "unsupported type");
			}
#	endif
		}


#endif




		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t countr_zero_normal(const T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				const std::uint64_t low = static_cast<std::uint64_t>(t);
				const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);

				return
					low == 0 ?
					64 + countr_zero_normal(high) :
					countr_zero_normal(low);
			}
			else
#endif
			if constexpr (sizeof(T) == 8) {
#if XER_ARCH(64)
#	if XER_HAS_BUILTIN(__builtin_ctzll)
				return t == 0 ? 64 : __builtin_ctzll(t);
#	elif XER_HAS_BUILTIN(__builtin_ctzg)
				return __builtin_ctzg(t, 64);
#	else
				return countr_zero_fallback(t);
#	endif
#elif XER_ARCH(32)

				const std::uint32_t low = static_cast<std::uint32_t>(t);
				const std::uint32_t high = static_cast<std::uint32_t>(t >> 32);

				return
					low == 0 ?
					32 + countr_zero_normal(high) :
					countr_zero_normal(low);
#endif
			}
			else if constexpr (sizeof(T) == 4) {
#	if XER_HAS_BUILTIN(__builtin_ctz)
				return t == 0 ? 32 : __builtin_ctz(t);
#	elif XER_HAS_BUILTIN(__builtin_ctzg)
				return __builtin_ctzg(t, 32);
#	else
				return countr_zero_fallback(t);
#	endif
			}
			else if constexpr (sizeof(T) == 2) {
#	if XER_HAS_BUILTIN(__builtin_ctz)
				constexpr std::uint32_t mask = 0xFFFF0000;
				return t == 0 ? 16 : __builtin_ctz(t|mask);
#	elif XER_HAS_BUILTIN(__builtin_ctzg)
				return __builtin_ctzg(t, 16);
#	else
				return countr_zero_fallback(t);
#	endif
			}
			else if constexpr (sizeof(T) == 1) {
#	if XER_HAS_BUILTIN(__builtin_ctz)
				constexpr std::uint32_t mask = 0xFFFFFF00;
				return t == 0 ? 8 : __builtin_ctz(t | mask);
#	elif XER_HAS_BUILTIN(__builtin_ctzg)
				return __builtin_ctzg(t, 8);
#	else
				return countr_zero_fallback(t);
#	endif
			}
			else {
				static_assert(false, "unsupported type");
				return 0;
			}
		}



#if XER_COMPILER(CLANG)
		template<typename T>
		requires bit_manipulatable<T>
		std::uint32_t countr_zero_clang(const T t)noexcept {
			return countr_zero_normal(t);
		}
#endif


#if XER_COMPILER(MSVC)
		template<typename T>
			requires bit_manipulatable<T>
		std::uint32_t countr_zero_msvc(const T t)noexcept {
#if XER_HAS_BUILTIN_INT128
			if constexpr (sizeof(T) == 16) {
				const std::uint64_t low = static_cast<std::uint64_t>(t);
				const std::uint64_t high = static_cast<std::uint64_t>(t >> 64);

				return
					low == 0 ?
					64 + countr_zero_msvc(high) :
					countr_zero_msvc(low);
			}
			else
#endif
				if constexpr (sizeof(T) == 8) {
#if XER_ARCH(64)
					unsigned long index;
					return
						_BitScanForward64(&index, t) == 0 ? 64 : index;
#elif XER_ARCH(32)

					const std::uint32_t low = static_cast<std::uint32_t>(t);
					const std::uint32_t high = static_cast<std::uint32_t>(t >> 32);

					return
						low == 0 ?
						32 + countr_zero_msvc(high) :
						countr_zero_msvc(low);
#endif
				}
				else if constexpr (sizeof(T) == 4) {
					unsigned long index;
					return
						_BitScanForward(&index, t) == 0 ? 32 : index;
				}
				else if constexpr (sizeof(T) == 2) {
					constexpr std::uint32_t mask = 0xFFFF0000;

					unsigned long index;
					return
						_BitScanForward(&index, t|mask) == 0 ? 16 : index;
				}
				else if constexpr (sizeof(T) == 1) {
					constexpr std::uint32_t mask = 0xFFFFFF00;

					unsigned long index;
					return
						_BitScanForward(&index, t | mask) == 0 ? 8 : index;
				}
				else {
					static_assert(false, "unsupported type");
					return 0;
				}
		}
#endif


	}

	template<typename T>
		requires details_::bit_manipulatable<T>
	inline constexpr std::uint32_t countr_zero(const T t)noexcept {
		if consteval {
			return details_::countr_zero_fallback(t);
		}
		else {
#if XER_BMI1_AVAILABLE
			return details_::countr_zero_bmi1(t);
#elif XER_COMPILER(GCC)
			return details_::countr_zero_gcc(t);
#else
#	if XER_X86_INTRIN_AVAILABLE
			if (is_tzcnt_supported()) {
				return details_::countr_zero_tzcnt(t);
			}
			else
#	endif
			{
#	if XER_COMPILER(MSVC)
				return details_::countr_zero_msvc(t);
#	elif XER_COMPILER(CLANG)
				return details_::countr_zero_clang(t);
#	else
				return details_::countr_zero_normal(t);
#	endif
			}
#endif
		}
	}

}

#endif