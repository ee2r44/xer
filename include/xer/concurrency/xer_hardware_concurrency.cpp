#include "xer_hardware_concurrency.h"

#include "../macro/xer_macro.h"

#include <cstddef>

#if XER_PLATFORM(WINDOWS)
#	include <Windows.h>
#elif XER_PLATFORM(POSIX)
#	include <unistd.h>
#endif

namespace xer {
	std::size_t hardware_concurrency()noexcept {
#if XER_PLATFORM(WINDOWS)
		const DWORD cores = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#elif XER_PLATFORM(POSIX)
		long cores = sysconf(_SC_NPROCESSORS_ONLN);
#else
		const std::size_t cores = 1;
#endif
		return cores > 0 ? cores : 1;
	}
}