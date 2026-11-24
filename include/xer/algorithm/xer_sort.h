#ifndef XER_SORT_H_
#define XER_SORT_H_

#include <cstddef>
#include <cmath>
#include <type_traits>
#include "../container/xer_vector.h"
#include "../macro/xer_macro.h"
#include "../utility/xer_move.h"
#include "../container/xer_span.h"
#include "xer_algorithm_details.h"
#include "xer_swap.h"


namespace xer {
	namespace details_ {
		template<typename T>
		concept sort_requirement =
			gt_judgable<T> &&
			std::is_nothrow_move_constructible_v<T> &&
			std::is_nothrow_move_assignable_v<T> &&
			std::is_nothrow_destructible_v<T>;

		template<typename T>
		concept sort_requirement2 =
			sort_requirement<T> &&
			ls_judgable<T>;
	}



	namespace details_ {

		template<typename T>
		requires
		details_::sort_requirement<T>
		constexpr void insert_sort_fallback(T*const XER_RESTRICT p,const std::size_t N)noexcept {
			for (std::size_t i = 1; i < N; i++) {
				T temp{ ::xer::move(p[i]) };
				std::size_t j = i;
				while (j > 0) {
					const std::size_t k = j - 1;
					if (p[k] > temp) {
						p[j] = ::xer::move(p[k]);
						j = k;
					}
					else break;
				}
				p[j] = ::xer::move(temp);
			}
		}
	}

	template<typename T>
	requires 
	details_::sort_requirement<T>
	constexpr void insert_sort(T*const XER_RESTRICT p,const std::size_t N)noexcept {
		details_::insert_sort_fallback(p,N);
	}


	namespace details_ {
		template<typename T>
		requires sort_requirement<T>
		constexpr void heap_sort(T* const XER_RESTRICT p, const std::size_t N)noexcept {
			for (std::size_t i = N >> 1; i > 0;) {
				i--;

				T temp{ ::xer::move(p[i]) };

				std::size_t j = i;
				std::size_t c = (i << 1) + 1;
				while (c + 1 < N) {
					if (p[c + 1] > p[c])c++;
					if (!(p[c] > temp))break;


					p[j] = ::xer::move(p[c]);
					j = c;
					c = (c << 1) + 1;
				}

				if (c < N && p[c]>temp) {
					p[j] = ::xer::move(p[c]);
					j = c;
				}

				p[j] = ::xer::move(temp);
			}

			for (std::size_t i = N; i > 1;) {
				i--;

				T temp{ ::xer::move(p[i]) };
				p[i] = ::xer::move(p[0]);

				std::size_t j = 0;
				std::size_t c = 1;

				while (c + 1 < i) {
					if (p[c + 1] > p[c])c++;
					if (!(p[c] > temp))break;

					p[j] = ::xer::move(p[c]);
					j = c;
					c = (c << 1) + 1;
				}

				if (c < i && p[c]>temp) {
					p[j] = ::xer::move(p[c]);
					j = c;
				}

				p[j] = ::xer::move(temp);
			}
		}

	}


	template<typename T>
	requires details_::sort_requirement<T>
	constexpr void heap_sort(T* const XER_RESTRICT p, const std::size_t N)noexcept {
		return details_::heap_sort(p, N);
	}



	template<typename T>
	requires 
	details_::sort_requirement2<T>&&
	std::is_nothrow_copy_constructible_v<T>
	constexpr bool sort(T* const XER_RESTRICT p, const std::size_t N)noexcept {
		if consteval {
			::xer::heap_sort(p, N);
			return true;
		}
		else {
			constexpr std::size_t ISGATE = 32;
			const std::size_t depth_gate = static_cast<std::size_t>(std::log2f(static_cast<float>(N + 1))) << 1;

			vector<std::size_t>stack;
			if (N > ISGATE && (!stack.reserve(depth_gate)))return false;

			std::size_t l = 0;
			std::size_t r = N;
			while (true) {
				const std::size_t rsl = r - l;
				if (rsl <= ISGATE) {
					::xer::insert_sort(p + l, rsl);
					if (stack.is_empty())break;
					else {
						l = r;
						r = stack.pop_out_unchecked();
					}
				}
				else if (stack.size()==depth_gate) {
					::xer::details_::heap_sort(p + l, rsl);
					if (stack.is_empty())break;
					else {
						l = r;
						r = stack.pop_out_unchecked();
					}
				}
				else {// r - l > ISGATE && stack.size() < depth_gate
					std::size_t i = l;
					std::size_t j = r - 1;

					const std::size_t middle = l + (rsl >> 1);
					if (p[i] > p[j])::xer::details_::swap_unchecked(p[i], p[j]);
					if (p[middle] < p[i])::xer::details_::swap_unchecked(p[middle], p[i]);
					if (p[middle] > p[j])::xer::details_::swap_unchecked(p[middle], p[j]);
					//p[i] <= pivot && p[j] >= pivot

					const T pivot{ p[middle] };

					while (true) {
						do { i++; } while (p[i] < pivot);
						do { j--; } while (p[j] > pivot);

						//p[i]>=pivot && p[j] <= pivot
						if (i >= j)break;
						//i<j
						::xer::details_::swap_unchecked(p[i], p[j]);

						//p[i]<=pivot && p[i] >= pivot
					}

					stack.push_back_unchecked(r);
					r = i;
				}
			}

			return true;
		}
	}

}


#endif