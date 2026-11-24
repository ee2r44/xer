#ifndef XER_MAX_H_
#define XER_MAX_H_

#include <type_traits>

namespace xer {
	namespace details_ {
		template<typename T>
		concept max_judgable = requires(const T & a, const T & b) {
			{ a > b }noexcept;
			requires std::is_same_v<decltype(a > b), bool>;
		};
	}

	template<typename T>
	requires details_::max_judgable<T>
	constexpr const T& max(const T& a, const T& b)noexcept {
		return a > b ? a : b;
	}
}

#endif