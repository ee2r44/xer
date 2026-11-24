#ifndef XER_VECTOR_H_
#define XER_VECTOR_H_

#include <type_traits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "../macro/xer_macro.h"
#include "../memory/xer_raw_alloc.h"
#include "../algorithm/xer_max.h"
#include "../memory/xer_construct.h"
#include "../utility/xer_move.h"
#include "../utility/xer_forward.h"

namespace xer {
	template<typename T>
	requires 
	std::is_object_v<T>&&
	(!std::is_array_v<T>)&&
	(sizeof(T)>0)
	class vector {
	private:
		vector(const vector&) = delete;
		vector& operator=(const vector&) = delete;
	private:
		T* ptr_;
		std::size_t size_;
		std::size_t capacity_;


		//assume new_cap>capacity_
		bool expand_unchecked(std::size_t new_cap)noexcept {
			new_cap = max(new_cap, capacity_ << 1);
			if constexpr (sizeof(T) > 1) {
				constexpr std::size_t mask = (~static_cast<std::size_t>(0)) / sizeof(T);
				if (new_cap > mask)return false;
			}

			if constexpr (reallocable<T>) {
				T* const temp = static_cast<T*>(std::realloc(ptr_, sizeof(T) * new_cap));
				if (temp == nullptr)XER_UNLIKELY return false;
				ptr_ = temp;
			}
			else {
				T* const temp = raw_alloc<T>(new_cap);
				if (temp == nullptr)XER_UNLIKELY return false;
				transfer_construct_n(temp, ptr_, size_);
				raw_free(ptr_);
				ptr_ = temp;
			}


			capacity_ = new_cap;
			return true;
		}



	public:
		constexpr vector()noexcept :
			ptr_(nullptr),
			size_(0),
			capacity_(0) {
		}

		constexpr vector(vector&& v)noexcept :
			ptr_(v.ptr_),
			size_(v.size_),
			capacity_(v.capacity_) {
			v.ptr_ = nullptr;
			v.size_ = 0;
			v.capacity_ = 0;
		}

		constexpr vector& operator=(vector&& v)noexcept {
			if (this != &v) {
				if (ptr_ != nullptr) {
					clear();
					raw_free(ptr_);
				}

				ptr_ = v.ptr_;
				size_ = v.size_;
				capacity_ = v.capacity_;

				v.ptr_ = nullptr;
				v.size_ = 0;
				v.capacity_ = 0;
			}
			return *this;
		}

		~vector()noexcept {
			if (ptr_ != nullptr) {
				clear();
				raw_free(ptr_);
			}
		}


		void clear()noexcept {
			static_assert(std::is_nothrow_destructible_v<T>, "vector-error");
			if constexpr (!std::is_trivially_destructible_v<T>) {
				destroy_n(ptr_, size_);//size_ >0 => ptr_ >0
			}
			size_ = 0;
		}

		constexpr std::size_t size()const noexcept { return size_; }
		constexpr std::size_t capacity()const noexcept { return capacity_; }
		constexpr bool is_empty()const noexcept { return size_ == 0; }

		T* data()noexcept { return ptr_; }
		const T* data()const noexcept { return ptr_; }
		T* begin()noexcept { return ptr_; }
		T* end()noexcept { return ptr_ + size_; }
		const T* begin()const noexcept { return ptr_; }
		const T* end()const noexcept { return ptr_ + size_; }


		bool reserve(const std::size_t new_cap)noexcept {
			return new_cap <= capacity_ || expand_unchecked(new_cap);
		}

		bool shrink_to_fit()noexcept {
			if (size_ < capacity_) {
				if (size_ == 0) {
					raw_free(ptr_);
					ptr_ = nullptr;
					capacity_ = 0;
				}
				else {//size_>0
					if constexpr (reallocable<T>) {
						T* const temp = static_cast<T*>(std::realloc(ptr_, size_ * sizeof(T)));
						if (temp == nullptr)XER_UNLIKELY return false;
						ptr_ = temp;
					}
					else {
						T* const temp = raw_alloc<T>(size_);
						if (temp == nullptr)XER_UNLIKELY return false;
						transfer_construct_n(temp, ptr_, size_);
						raw_free(ptr_);
						ptr_ = temp;
					}
					capacity_ = size_;
				}
			}

			return true;
		}


		//assume capacity is enough
		void push_back_unchecked(const T& t)noexcept {
			static_assert(std::is_nothrow_copy_constructible_v<T>, "vector-error");
			construct_at(ptr_ + size_, t);
			size_++;
		}

		//assume capacity is enough
		void push_back_unchecked(T&& t)noexcept {
			static_assert(std::is_nothrow_move_constructible_v<T>, "vector-error");
			construct_at(ptr_ + size_, forward<T>(t));
			size_++;
		}

		template<typename...Args>
		void emplace_back_unchecked(Args&&...args)noexcept {
			static_assert(std::is_nothrow_constructible_v<T, Args...>, "vector-error");
			construct_at(ptr_ + size_, forward<Args>(args)...);
			size_++;
		}


		bool resize(const std::size_t new_size)noexcept {
			if (new_size > size_) {
				if (new_size > capacity_ && (!expand_unchecked(new_size)))return false;

				if constexpr (std::is_trivially_default_constructible_v<T>) {
					std::memset(ptr_ + size_, 0, sizeof(T) * (new_size - size_));
				}
				else {
					static_assert(std::is_nothrow_default_constructible_v<T>, "vector-error");
					T* const End = ptr_ + new_size;
					for (T* p = ptr_ + size_; p < End; p++) {
						construct_at(p);
					}
				}
			}
			else {//new_size<=size_
				if constexpr (!std::is_trivially_destructible_v<T>) {
					static_assert(std::is_nothrow_destructible_v<T>, "vector-error");
					destroy_n(ptr_ + new_size, size_ - new_size);
				}
			}

			size_ = new_size;

			return true;

		}

		bool erase(const std::size_t index)noexcept {
			if (index >= size_)return false;

			erase_unchecked(index);
			return true;
		}

		//assume 0<=index <size_
		void erase_unchecked(const std::size_t index)noexcept {
			static_assert(std::is_nothrow_destructible_v<T>, "vector-error");
			size_--;

			if constexpr (std::is_trivially_copyable_v<T>) {
				T* const p = ptr_ + index;
				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(p);
				}
				std::memmove(p, p + 1, sizeof(T) * (size_ - index));

				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(ptr_ + size_);
				}
			}
			else if constexpr (std::is_nothrow_move_assignable_v<T>) {
				T* const End = ptr_ + size_;
				T* p = ptr_ + index;
				for (; p < End; p++) {
					*p = move(*(p + 1));
				}

				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(p);
				}
			}
			else if constexpr (std::is_nothrow_move_constructible_v<T>) {
				T* const End = ptr_ + size_;
				T* p = ptr_ + index;
				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(p);
				}
				for (; p < End; p++) {
					construct_at(p, move(*(p + 1)));
					if constexpr (!std::is_trivially_destructible_v<T>) {
						destroy_at(p + 1);
					}
				}
			}
			else if constexpr (std::is_nothrow_copy_assignable_v<T>) {
				T* const End = ptr_ + size_;
				T* p = ptr_ + index;
				for (; p < End; p++) {
					*p = *(p + 1);
				}

				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(p);
				}
			}
			else if constexpr (std::is_nothrow_copy_constructible_v<T>) {
				T* const End = ptr_ + size_;
				T* p = ptr_ + index;
				if constexpr (!std::is_trivially_destructible_v<T>) {
					destroy_at(p);
				}
				for (; p < End; p++) {
					construct_at(p, *(p + 1));
					if constexpr (!std::is_trivially_destructible_v<T>) {
						destroy_at(p + 1);
					}
				}
			}
			else {
				static_assert(false, "vector-error");
			}
		}

		bool unordered_erase(const std::size_t index)noexcept {
			if (index >= size_)return false;
			unordered_erase_unchecked(index);
			return true;
		}

		void unordered_erase_unchecked(const std::size_t index)noexcept {
			static_assert(std::is_nothrow_destructible_v<T>, "vector-error");
			size_--;

			T* const src = ptr_ + size_;

			if (index < size_) {
				T* const dst = ptr_ + index;

				if constexpr (std::is_nothrow_move_assignable_v<T>) {
					*dst = move(*src);
				}
				else if constexpr (std::is_nothrow_move_constructible_v<T>) {
					if constexpr (!std::is_trivially_destructible_v<T>) {
						destroy_at(dst);
					}
					construct_at(dst, move(*src));
				}
				else if constexpr (std::is_nothrow_copy_assignable_v<T>) {
					*dst = *src;
				}
				else if constexpr (std::is_nothrow_copy_constructible_v<T>) {
					if constexpr (!std::is_trivially_destructible_v<T>) {
						destroy_at(dst);
					}
					construct_at(dst, *src);
				}
				else {
					static_assert(false, "vector-error");
				}
			}

			if constexpr (!std::is_trivially_destructible_v<T>) {
				destroy_at(src);
			}
		}

		bool erase_last()noexcept {
			if (size_ == 0)return false;
			erase_last_unchecked();
			return true;
		}

		void erase_last_unchecked()noexcept {
			size_--;
			if constexpr (!std::is_trivially_destructible_v<T>) {
				static_assert(std::is_nothrow_destructible_v<T>, "vector-error");
				destroy_at(ptr_ + size_);
			}
		}

		T pop_out_unchecked()noexcept {
			size_--;
			if constexpr (std::is_trivially_destructible_v<T>) {
				return move(*(ptr_ + size_));
			}
			else if constexpr (std::is_nothrow_constructible_v<T, T>) {
				T result = move(*(ptr_ + size_));
				destroy_at(ptr_ + size_);
				return result;
			}
			else {
				static_assert(false, "vector-error");
			}
		}

		template<typename Index_t>
			requires std::is_integral_v<Index_t>
		T& operator [](const Index_t index)noexcept {
			return ptr_[index];
		}

		template<typename Index_t>
			requires std::is_integral_v<Index_t>
		const T& operator [](const Index_t index)const noexcept {
			return ptr_[index];
		}
	};
}


#endif