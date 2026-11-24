#ifndef XER_CONSTRUCT_H_
#define XER_CONSTRUCT_H_

#include <cstddef>
#include <cstring>
#include <type_traits>
#include <new>

#include "../macro/xer_macro.h"
#include "../utility/xer_move.h"
#include "../utility/xer_forward.h"
#include "xer_addressof.h"

namespace xer {
	template<typename T, typename...Args>
	requires std::is_nothrow_constructible_v<T, Args...> && (!std::is_array_v<T>)
	void construct_at(T* const XER_RESTRICT ptr, Args&&...args)noexcept {
		::new (static_cast<void*>(ptr)) T(::xer::forward<Args>(args)...);
	}

	template<typename T>
	requires 
	std::is_nothrow_destructible_v<T>
	constexpr void destroy_at(T* const XER_RESTRICT ptr)noexcept {
		if constexpr (!std::is_trivially_destructible_v<T>) {
			if constexpr (std::is_array_v<T>) {
				for (auto& a : *ptr) {
					::xer::destroy_at(::xer::addressof(a));
				}
			}
			else {
				ptr->~T();
			}
		}
	}


	//can take count==0 as input
	template<typename T>
	requires 
	std::is_nothrow_destructible_v<T>
	constexpr void destroy_n(T* XER_RESTRICT ptr, const std::size_t count)noexcept {
		if constexpr (!std::is_trivially_destructible_v<T>) {
			T* const end = ptr + count;
			for (; ptr < end; ptr++) {
				::xer::destroy_at(ptr);
			}
		}
	}


	template<typename T>
		requires
	std::is_object_v<T> &&
	(!std::is_array_v<T>) &&
	std::is_nothrow_destructible_v<T> &&
	(
	std::is_trivially_copyable_v<T> ||
	std::is_nothrow_move_constructible_v<T> ||
	std::is_nothrow_copy_constructible_v<T>
	)
	void transfer_construct_at(T* const XER_RESTRICT dst, T& XER_RESTRICT src)noexcept {
		if constexpr (std::is_trivially_copyable_v<T>) {
			std::memcpy(dst, ::xer::addressof(src), sizeof(T));
		}
		else if constexpr (std::is_nothrow_move_constructible_v<T>) {
			::xer::construct_at(dst, ::xer::move(src));
		}
		else{
			::xer::construct_at(dst, src);
		}

		if constexpr (!std::is_trivially_destructible_v<T>) {
			::xer::destroy_at(::xer::addressof(src));
		}
	}


	//need to insure count*sizeof(T) don't overflow
	template<typename T>
	requires
	std::is_object_v<T>&&
	(!std::is_array_v<T>)&&
	std::is_nothrow_destructible_v<T>
	void transfer_construct_n(T* const XER_RESTRICT dst, T* const XER_RESTRICT src, const std::size_t count)noexcept {
		if constexpr (std::is_trivially_copyable_v<T>) {
			if (count == 0)return;
			std::memcpy(dst, src, sizeof(T) * count);

			if constexpr (!std::is_trivially_destructible_v<T>) {
				::xer::destroy_n(src, count);
			}
		}
		else if constexpr (
#if (XER_COMPILER(CLANG)||XER_COMPILER(GCC))&&XER_CPU(X86_64)
#  if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wdeprecated-builtins"
#  endif
			__has_trivial_copy(T) && std::is_nothrow_copy_constructible_v<T>
#  if defined(__clang__)
#    pragma clang diagnostic pop
#  endif
#else
			false
#endif
			) {
			for (std::size_t i = 0; i < count; i++) {
				T* const S = src + i;
				::xer::construct_at(dst + i, *S);
				if constexpr (!std::is_trivially_destructible_v<T>) {
					::xer::destroy_at(S);
				}
			}
		}
		else if constexpr (std::is_nothrow_move_constructible_v<T>) {

#if XER_COMPILER(CLANG)&&XER_CPU(X86_64)
			if constexpr (sizeof(T) <= 16) {
				for (std::size_t i = 0; i < count; i++) {
					T* const S = src + i;
					::xer::construct_at(dst + i, ::xer::move(*S));
					if constexpr (!std::is_trivially_destructible_v<T>) {
						::xer::destroy_at(S);
					}
				}
			}
			else {
				for (std::size_t i = 0; i < count; i++) {
					::xer::construct_at(dst + i, ::xer::move(src[i]));
				}
				if constexpr (!std::is_trivially_destructible_v<T>) {
					::xer::destroy_n(src, count);
				}
			}
#elif XER_COMPILER(GCC)&&XER_CPU(X86_64)
			for (std::size_t i = 0; i < count; i++) {
				T* const S = src + i;
				::xer::construct_at(dst + i, ::xer::move(*S));
				if constexpr (!std::is_trivially_destructible_v<T>) {
					::xer::destroy_at(S);
				}
			}
#else
			for (std::size_t i = 0; i < count; i++) {
				::xer::construct_at(dst + i, ::xer::move(src[i]));
			}
			if constexpr (!std::is_trivially_destructible_v<T>) {
				::xer::destroy_n(src, count);
			}
#endif
		}
		else if constexpr (std::is_nothrow_copy_constructible_v<T>) {
			for (std::size_t i = 0; i < count; i++) {
				::xer::construct_at(dst + i, src[i]);
			}
			if constexpr (!std::is_trivially_destructible_v<T>) {
				::xer::destroy_n(src, count);
			}
		}
		else {
			static_assert(false, "transfer-construct-n error");
		}
	}
}

#endif