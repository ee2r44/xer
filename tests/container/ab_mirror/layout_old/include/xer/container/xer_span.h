#ifndef XER_SPAN_H_
#define XER_SPAN_H_

#include <type_traits>
#include <cstddef>

namespace xer {
	template<typename T, bool is_const = true>
	requires 
	std::is_object_v<T>&&
	(!std::is_array_v<T>)
	class span {
	private:
		using Ptr_t = std::conditional_t<is_const, const T*, T*>;

		using Ref_t = std::conditional_t<is_const, const T&, T&>;

		Ptr_t ptr_;
		std::size_t size_;
	public:
		constexpr span()noexcept :
			ptr_(nullptr),
			size_(0) {
		}
		constexpr span(const span& s)noexcept = default;
		constexpr span(span&& s)noexcept = default;
		constexpr span& operator=(const span& s)noexcept = default;
		constexpr span& operator=(span&& s)noexcept = default;
		constexpr ~span()noexcept = default;
		constexpr span(const Ptr_t ptr_, const std::size_t size_)noexcept :
			ptr_(ptr_),
			size_(size_) {
		}
		constexpr span(const span<T, false>& s) noexcept
		requires (is_const): 
		ptr_(s.data()), size_(s.size()) {
		}

		constexpr span& operator=(const span<T, false>& s) noexcept requires (is_const) {
			ptr_ = s.data();
			size_ = s.size();
			return *this;
		}

		constexpr bool is_empty()const noexcept { return size_ == 0; }
		constexpr std::size_t size()const noexcept { return size_; }
		constexpr Ptr_t data()const noexcept { return ptr_; }
		constexpr Ptr_t begin()const noexcept { return ptr_; }
		constexpr Ptr_t end()const noexcept { return ptr_ + size_; }



		template<typename Index_t>
		requires std::is_integral_v<Index_t>
		constexpr Ref_t operator [](const Index_t index)const noexcept {
			return ptr_[index];
		}
	};
}


#endif