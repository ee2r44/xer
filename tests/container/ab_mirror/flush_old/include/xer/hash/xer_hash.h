#ifndef XER_HASH_H_
#define XER_HASH_H_

#include <cstddef>
#include <type_traits>

namespace xer {
	template<typename HS,typename Key>
	concept hashable = requires(const HS & h, const Key & k) {
		requires std::is_nothrow_default_constructible_v<HS>;
		requires std::is_nothrow_constructible_v<HS, std::size_t>;
		requires std::is_nothrow_destructible_v<HS>;
		requires std::is_same_v<decltype(h(k)), std::size_t>;
	};
}

#endif