#ifndef XER_BIT_DETAILS_H_
#define XER_BIT_DETAILS_H_

#include <type_traits>

#include "../macro/xer_macro.h"

namespace xer {
	namespace details_ {
		template<typename T>
		concept bit_manipulatable =
#if XER_HAS_BUILTIN_INT128
			std::is_same_v<T, unsigned __int128> ||
#endif
			(std::is_unsigned_v<T> &&
			(sizeof(T) != 0 && (sizeof(T) & (sizeof(T) - 1)) == 0) &&
			(!std::is_same_v<T, bool>));
	}
}

#endif