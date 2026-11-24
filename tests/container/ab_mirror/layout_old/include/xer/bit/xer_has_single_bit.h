#ifndef XER_HAS_SINGLE_BIT_H_
#define XER_HAS_SINGLE_BIT_H_

#include "xer_bit_details.h"

namespace xer {
	template<typename T>
	requires details_::bit_manipulatable<T>
	constexpr bool has_single_bit(const T t)noexcept {
		return t != 0 && (t & (t - 1)) == 0;
	}
}

#endif