#ifndef XER_TO_UNDERLYING_H_
#define XER_TO_UNDERLYING_H_

#include <type_traits>

namespace xer {
	template<typename E>
	requires std::is_enum_v<E>
	constexpr std::underlying_type_t<E> to_underlying(const E e)noexcept {
		return static_cast<std::underlying_type_t<E>>(e);
	}
}

#endif