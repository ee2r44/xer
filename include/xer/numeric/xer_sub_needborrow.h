#ifndef XER_SUB_NEEDBORROW_H_
#define XER_SUB_NEEDBORROW_H_

#include "xer_numeric_details.h"

namespace xer {
	template<typename L,typename R>
	requires details_::sub_needborrowable<L,R>
	constexpr bool sub_needborrow(const L l, const R r)noexcept {
		return l < r;
	}


}


#endif