#include "xer_env.h"

#include "../macro/xer_macro.h"

#if XER_CPU(X86_64)||XER_CPU(X86_32)
#	if XER_COMPILER(MSVC)
#		include <intrin.h>
#	elif XER_COMPILER(GCC) || XER_COMPILER(CLANG)
#		include <cpuid.h>
#		include <immintrin.h>
#	endif
#endif

namespace xer {
	namespace details_ {
		namespace env_ {
			namespace {
#if (XER_CPU(X86_64)||XER_CPU(X86_32)) && (XER_COMPILER(MSVC) || XER_COMPILER(GCC) || XER_COMPILER(CLANG))
				// Intel/AMD：AVX 可用 = CPUID.AVX + OSXSAVE + XCR0[2:1]==11b（XMM|YMM 由 OS 启用）。
#	if XER_COMPILER(GCC) || XER_COMPILER(CLANG)
				__attribute__((target("xsave")))
#	endif
					static bool probe_avx_os_enabled(unsigned leaf1_ecx) noexcept {
					constexpr unsigned k_osxsave = 1u << 27;
					constexpr unsigned k_avx = 1u << 28;
					if ((leaf1_ecx & (k_osxsave | k_avx)) != (k_osxsave | k_avx)) {
						return false;
					}
					const unsigned long long xcr0 = _xgetbv(0);
					return (xcr0 & 0x6ull) == 0x6ull;
				}
#endif
			}

			env::env()noexcept :
				table_{} {
#if (XER_CPU(X86_64)||XER_CPU(X86_32)) && XER_COMPILER(MSVC)
				int info[4]{};
				__cpuid(info, 0);
				const int max_leaf = info[0];

				__cpuid(info, 1);
				const unsigned leaf1_ecx = static_cast<unsigned>(info[2]);

				table_[ssse3_support_] = (leaf1_ecx & (1u << 9)) != 0;
				table_[sse4_2_support_] = (leaf1_ecx & (1u << 20)) != 0;
				table_[popcnt_support_] = (leaf1_ecx & (1u << 23)) != 0;
				table_[avx_support_] = probe_avx_os_enabled(leaf1_ecx);

				bool cpu_avx2 = false;
				if (max_leaf >= 7) {
					__cpuidex(info, 7, 0);
					table_[bmi1_support_] = (info[1] & (1 << 3)) != 0;
					table_[bmi2_support_] = (info[1] & (1 << 8)) != 0;
					cpu_avx2 = (info[1] & (1 << 5)) != 0;
				}
				table_[avx2_support_] = table_[avx_support_] && cpu_avx2;

				__cpuid(info, static_cast<int>(0x80000000u));
				const unsigned max_ext = static_cast<unsigned>(info[0]);
				if (max_ext >= 0x80000001u) {
					__cpuid(info, static_cast<int>(0x80000001u));
					table_[lzcnt_support_] = (info[2] & (1 << 5)) != 0;
				}

#elif (XER_CPU(X86_64)||XER_CPU(X86_32)) && (XER_COMPILER(GCC) || XER_COMPILER(CLANG))
				unsigned eax = 0, ebx = 0, ecx = 0, edx = 0;
				if (__get_cpuid(1, &eax, &ebx, &ecx, &edx) != 0) {
					table_[ssse3_support_] = (ecx & (1u << 9)) != 0;
					table_[sse4_2_support_] = (ecx & (1u << 20)) != 0;
					table_[popcnt_support_] = (ecx & (1u << 23)) != 0;
					table_[avx_support_] = probe_avx_os_enabled(ecx);

					bool cpu_avx2 = false;
					if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) != 0) {
						table_[bmi1_support_] = (ebx & (1u << 3)) != 0;
						table_[bmi2_support_] = (ebx & (1u << 8)) != 0;
						cpu_avx2 = (ebx & (1u << 5)) != 0;
					}
					table_[avx2_support_] = table_[avx_support_] && cpu_avx2;

					if (__get_cpuid(0x80000001u, &eax, &ebx, &ecx, &edx) != 0) {
						table_[lzcnt_support_] = (ecx & (1u << 5)) != 0;
					}
				}

#endif
			}
		}
	}
}