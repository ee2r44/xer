#ifndef XER_FORWARD_H_
#define XER_FORWARD_H_

#include <type_traits>

namespace xer {
	template<typename T>
	constexpr T&& forward(std::remove_reference_t<T>& t)noexcept {
		return static_cast<T&&>(t);
	}

	template<typename T>
	constexpr T&& forward(std::remove_reference_t<T>&& t)noexcept {
		static_assert(!std::is_lvalue_reference_v<T>, "cannot forward an lvalue as an rvalue");
		return static_cast<T&&>(t);
	}
}


#endif