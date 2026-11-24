#ifndef XER_MAX_H_
#define XER_MAX_H_


#include "xer_algorithm_details.h"

namespace xer {

	template<typename T>
	requires details_::gt_judgable<T>
	constexpr const T& max(const T& a, const T& b)noexcept {
		return a > b ? a : b;
	}
}

#endif