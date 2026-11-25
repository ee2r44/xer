#ifndef XER_HARDWARE_CONCURRENCY_H_
#define XER_HARDWARE_CONCURRENCY_H_

#include <cstddef>

namespace xer {
	std::size_t hardware_concurrency()noexcept;
}

#endif