// A/B control mirror of include/xer/algorithm/xer_sort.h :: xer::sort
// Same as include except: no XER_PREFETCH_N on the qsort/partition branch.
// ISGATE = 32 (matches include). Does not modify include/.

#ifndef XER_SORT_AB_NOPREFETCH_H_
#define XER_SORT_AB_NOPREFETCH_H_

#include "xer/algorithm/xer_sort.h"

#include <cmath>
#include <cstddef>
#include <type_traits>

namespace xer_ab {

template <typename T>
requires xer::details_::sort_requirement2<T> &&
         std::is_nothrow_copy_constructible_v<T>
constexpr bool sort(T* const XER_RESTRICT p, const std::size_t N) noexcept {
	if consteval {
		::xer::heap_sort(p, N);
		return true;
	} else {
		constexpr std::size_t ISGATE = 32;
		const std::size_t depth_gate =
		    static_cast<std::size_t>(std::log2f(static_cast<float>(N + 1))) << 1;

		xer::vector<std::size_t> stack;
		if (N > ISGATE && (!stack.reserve(depth_gate))) return false;

		std::size_t l = 0;
		std::size_t r = N;
		while (true) {
			const std::size_t rsl = r - l;
			if (rsl <= ISGATE) {
				::xer::insert_sort(p + l, rsl);
				if (stack.is_empty()) break;
				else {
					l = r;
					r = stack.pop_out_unchecked();
				}
			} else if (stack.size() == depth_gate) {
				::xer::details_::heap_sort(p + l, rsl);
				if (stack.is_empty()) break;
				else {
					l = r;
					r = stack.pop_out_unchecked();
				}
			} else {
				std::size_t i = l;
				std::size_t j = r - 1;
				// control: no XER_PREFETCH_N(p + i, rsl);
				const std::size_t middle = l + (rsl >> 1);
				if (p[i] > p[j]) ::xer::details_::swap_unchecked(p[i], p[j]);
				if (p[middle] < p[i]) ::xer::details_::swap_unchecked(p[middle], p[i]);
				if (p[middle] > p[j]) ::xer::details_::swap_unchecked(p[middle], p[j]);

				const T pivot{p[middle]};

				while (true) {
					do {
						i++;
					} while (p[i] < pivot);
					do {
						j--;
					} while (p[j] > pivot);

					if (i >= j) break;
					::xer::details_::swap_unchecked(p[i], p[j]);
				}

				stack.push_back_unchecked(r);
				r = i;
			}
		}

		return true;
	}
}

}  // namespace xer_ab

#endif
