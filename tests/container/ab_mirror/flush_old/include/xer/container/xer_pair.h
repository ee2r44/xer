#ifndef XER_PAIR_H_
#define XER_PAIR_H_

#include <type_traits>

namespace xer {
	template<typename F,typename S>
	requires 
	std::is_object_v<F>&&
	std::is_object_v<S>
	struct pair {
		using first_type = F;
		using second_type = S;

		F first;
		S second;
	};
}

#endif