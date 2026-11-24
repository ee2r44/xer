#ifndef XER_LINEAR_ALGEBRA_DETAILS_H_
#define XER_LINEAR_ALGEBRA_DETAILS_H_

#include <type_traits>

namespace xer {
	namespace details_ {
		template<typename T>
		concept valid_linear_algebra_elem =
			(std::is_integral_v<T> && (!std::is_same_v<T, bool>)) ||
			std::is_floating_point_v<T>;
	}
}


#endif