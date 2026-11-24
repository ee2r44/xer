#ifndef XER_MOVE_H_
#define XER_MOVE_H_

#include <type_traits>

namespace xer {
	template<typename T>
	constexpr std::remove_reference_t<T>&& move(T&& t)noexcept {
		return static_cast<std::remove_reference_t<T>&&>(t);
	}
}

#endif