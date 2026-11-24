#ifndef XER_NUMERIC_DETAILS_H_
#define XER_NUMERIC_DETAILS_H_

#include <type_traits>
#include "../bit/xer_bit_details.h"

namespace xer {
	namespace details_ {
		template<typename L,typename R>
		concept add_overflowable =
			(bit_manipulatable<L> || std::is_same_v<L, bool>) &&
			(bit_manipulatable<R> || std::is_same_v<R, bool>) &&
			(!(std::is_same_v<L, bool> && std::is_same_v<R, bool>));

		template<typename L,typename R>
		concept sub_needborrowable =
			add_overflowable<L, R>;
	}
}

#endif