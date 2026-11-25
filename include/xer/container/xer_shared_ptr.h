#ifndef XER_SHARED_PTR_H_
#define XER_SHARED_PTR_H_


#include <cstddef>
#include <type_traits>
#include <atomic>

#include "../error/xer_valid_chain.h"
#include "../memory/xer_raw_alloc.h"
#include "../memory/xer_construct.h"
#include "../memory/xer_addressof.h"
#include "../utility/xer_forward.h"

namespace xer {
	static_assert(std::is_nothrow_constructible_v<std::atomic<std::size_t>, std::size_t>, "shared ptr error");

	template<typename T>
	requires 
	std::is_object_v<T>
	class shared_ptr {
	private:
		struct Page {
			std::atomic<std::size_t>counter_;
			T data_;
		};

		Page* ptr_;


		void try_sub()noexcept {
			if (ptr_ != nullptr) {
				const std::size_t judge =
					(ptr_->counter_).fetch_sub(
						static_cast<std::size_t>(1),
						std::memory_order::release);

				if (judge == 1) {
					std::atomic_thread_fence(std::memory_order::acquire);
					::xer::destroy_at(::xer::addressof(ptr_->data_));
					::xer::raw_free(ptr_);
				}
			}
		}
		
	public:

		shared_ptr()noexcept:
			ptr_(nullptr){}

		template<typename...Args>
		requires std::is_nothrow_constructible_v<T,Args...>
		shared_ptr(valid_chain& v, Args&&...args)noexcept:
		shared_ptr(){
			if (!v)return;
			ptr_ = ::xer::raw_alloc<Page>();
			if (ptr_ == nullptr) {
				v = false;
				return;
			}
			::xer::construct_at(
				::xer::addressof(ptr_->counter_),
				1
			);

			::xer::construct_at(
				::xer::addressof(ptr_->data_),
				::xer::forward<Args>(args)...);
		}


		shared_ptr(const shared_ptr& s)noexcept :
		ptr_(s.ptr_) {
			if (ptr_ != nullptr) {
				(ptr_->counter_).fetch_add(
					static_cast<std::size_t>(1),
					std::memory_order::relaxed);
			}
		}

		shared_ptr& operator=(const shared_ptr& s)noexcept {
			if (this != ::xer::addressof(s) && ptr_ != s.ptr_) {
				try_sub();
				ptr_ = s.ptr_;
				if (ptr_ != nullptr) {
					(ptr_->counter_).fetch_add(
						static_cast<std::size_t>(1),
						std::memory_order::relaxed);
				}
			}
			return *this;
		}

		shared_ptr(shared_ptr&& s)noexcept :
			ptr_(s.ptr_) {
			s.ptr_ = nullptr;
		}

		shared_ptr& operator=(shared_ptr&& s)noexcept {
			if (this != ::xer::addressof(s)) {
				try_sub();
				ptr_ = s.ptr_;
				s.ptr_ = nullptr;
			}
			return *this;
		}

		~shared_ptr()noexcept {
			try_sub();
		}

		T* operator->()noexcept {
			return ::xer::addressof(ptr_->data_);
		}
		const T* operator->()const noexcept {
			return ::xer::addressof(ptr_->data_);
		}

		T& operator*()noexcept {
			return ptr_->data_;
		}
		const T& operator*()const noexcept {
			return ptr_->data_;
		}
	};


	template<typename T,typename...Args>
	requires std::is_nothrow_constructible_v<shared_ptr<T>,valid_chain&,Args...>
	shared_ptr<T> make_shared(valid_chain& v, Args&&...args)noexcept {
		return shared_ptr<T>{v, ::xer::forward<Args>(args)...};
	}
}


#endif