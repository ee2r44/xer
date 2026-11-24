#ifndef XER_SWAP_H_
#define XER_SWAP_H_

#include <type_traits>
#include "../macro/xer_macro.h"
#include "../utility/xer_move.h"

namespace xer {
	namespace details_{
		template<typename T>
		concept swap_requirement =
			std::is_nothrow_move_constructible_v<T> &&
			std::is_nothrow_move_assignable_v<T> &&
			std::is_nothrow_destructible_v<T>;


		template<typename T>
			requires details_::swap_requirement<T>
		void swap_unchecked(T&XER_RESTRICT a, T&XER_RESTRICT b)noexcept {
			T temp{ ::xer::move(a) };
			a = ::xer::move(b);
			b = ::xer::move(temp);
		}
	}

	template<typename T>
	requires details_::swap_requirement<T>
	void swap(T& a, T& b)noexcept {
		T temp{ ::xer::move(a) };
		a = ::xer::move(b);
		b = ::xer::move(temp);
	}
}

#endif