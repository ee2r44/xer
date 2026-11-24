#ifndef XER_RET_H_
#define XER_RET_H_

#include <cstdint>

namespace xer {
	enum class ret :std::uint32_t {
		success = 0,
		error,
		alloc_failure,
		value_collison,
		value_no_found,
		out_of_space
	};
}


#endif