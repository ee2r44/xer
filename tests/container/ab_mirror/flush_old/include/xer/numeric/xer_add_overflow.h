#ifndef XER_ADD_OVERFLOW_H_
#define XER_ADD_OVERFLOW_H_

#include "xer_numeric_details.h"

namespace xer {
	template<typename L,typename R>
	requires details_::add_overflowable<L,R>
	constexpr bool add_overflow(const L l, const R r)noexcept {
		using B = std::conditional_t<(sizeof(L) > sizeof(R)), L, R>;


		constexpr B mask = ~static_cast<B>(0);
		return (mask ^ l) < r;
	}
}

#endif