#ifndef XER_ARRAY_H_
#define XER_ARRAY_H_

#include <cstddef>
#include <type_traits>

namespace xer {
	template<typename T,std::size_t N>
	requires
	std::is_object_v<T>&&
	(!std::is_array_v<T>)&&
	(N>0)
	struct array {
		T elems_[N];


		constexpr std::size_t size()const noexcept { return N; }
		constexpr T* data()noexcept { return elems_; }
		constexpr const T* data()const noexcept { return elems_; }
		constexpr T* begin() noexcept { return elems_; }
		constexpr const T* begin()const noexcept { return elems_; }
		constexpr T* end()noexcept { return elems_ + N; }
		constexpr const T* end()const noexcept { return elems_ + N; }



		template<typename Index_t>
			requires std::is_integral_v<Index_t>
		constexpr T& operator [](const Index_t index)noexcept {
			return elems_[index];
		}

		template<typename Index_t>
			requires std::is_integral_v<Index_t>
		constexpr const T& operator [](const Index_t index)const noexcept {
			return elems_[index];
		}
	};


	static_assert(std::is_trivial_v<int>&& std::is_trivial_v<array<int, 2>>,
		"array<trivial-type,N> must be trivial type");

}

#endif