#ifndef XER_MIN_H_
#define XER_MIN_H_

#include <type_traits>

#include "xer_algorithm_details.h"

namespace xer {

	template<typename T>
	requires details_::ls_judgable<T>
	constexpr const T& min(const T& a, const T& b)noexcept {
		return a < b ? a : b;
	}
}

#endif