#ifndef XER_RAW_ALLOC_H_
#define XER_RAW_ALLOC_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "../macro/xer_macro.h"
#include "../bit/xer_has_single_bit.h"
#include "../numeric/xer_add_overflow.h"


#if XER_PLATFORM(WINDOWS)
#	include <malloc.h>
#elif (!XER_HAS_ALIGNED_ALLOC)&&XER_PLATFORM(POSIX)
#include <stdlib.h>
#endif

/*
* for alloc/free of raw memory
*/
namespace xer {
	namespace details_ {
		template<typename T>
		concept raw_allocable =
			std::is_object_v<T> && (sizeof(T) > 0)&&(!std::is_array_v<T>);

		inline constexpr std::size_t raw_alloc_basic_alignment = alignof(std::max_align_t);

		static_assert(
			has_single_bit(raw_alloc_basic_alignment) &&
			raw_alloc_basic_alignment >= sizeof(std::size_t) &&
			raw_alloc_basic_alignment >= sizeof(std::uintptr_t),
			"alloc alignment error"
			);

#if (!XER_PLATFORM(WINDOWS))&&(!XER_PLATFORM(POSIX))
		template<std::size_t align>
		requires
		(align > raw_alloc_basic_alignment) &&
		(has_single_bit(align))
		void* aligned_alloc_fallback(const std::size_t size)noexcept {
			constexpr std::uintptr_t mask0 = align - 1;
			constexpr std::uintptr_t mask1 = ~static_cast<std::uintptr_t>(mask0);
			if (add_overflow(size, align))return nullptr;


			void* const ptr0 = std::malloc(size + align);
			if (ptr0 == nullptr)XER_UNLIKELY return nullptr;


			const std::uintptr_t ptr1 = (reinterpret_cast<std::uintptr_t>(ptr0) + raw_alloc_basic_alignment + mask0) & mask1;
			*reinterpret_cast<std::uintptr_t*>(ptr1 - sizeof(std::uintptr_t)) = ptr1 - reinterpret_cast<std::uintptr_t>(ptr0);

			return reinterpret_cast<void*>(ptr1);
		}

		void aligned_free_fallback(void* const ptr)noexcept {
			if (ptr == nullptr)return;
			const std::uintptr_t p =
				reinterpret_cast<std::uintptr_t>(ptr)
				- *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(ptr) - sizeof(std::uintptr_t));
			std::free(reinterpret_cast<void*>(p));
		}
#endif

		template<std::size_t align>
		requires
		(has_single_bit(align))
		void* aligned_alloc_raw(const std::size_t size)noexcept {
			if constexpr (align <= details_::raw_alloc_basic_alignment) {
				return std::malloc(size);
			}
			else {
#if XER_PLATFORM(WINDOWS)
				return _aligned_malloc(size, align);
#elif XER_HAS_ALIGNED_ALLOC
				return std::aligned_alloc(align, size);
#elif XER_PLATFORM(POSIX)
				void* ptr;
				return
					posix_memalign(&ptr, align, size) == 0 ? ptr : nullptr;
#else
				return details_::aligned_alloc_fallback<align>(size);
#endif
			}
		}

		template<std::size_t align>
		requires
		(has_single_bit(align))
		void aligned_free_raw(void* const p)noexcept {
			if constexpr (align <= details_::raw_alloc_basic_alignment) {
				std::free(p);
			}
			else {
#if XER_PLATFORM(WINDOWS)
				_aligned_free(p);
#elif XER_HAS_ALIGNED_ALLOC
				std::free(p);
#elif XER_PLATFORM(POSIX)
				std::free(p);
#else
				details_::aligned_free_fallback(p);
#endif
			}
		}
	}


	template<typename T>
	requires details_::raw_allocable<T>
	T* raw_alloc()noexcept {
		return 
			static_cast<T*>(details_::aligned_alloc_raw<alignof(T)>(sizeof(T)));
	}

	template<typename T>
		requires details_::raw_allocable<T>
	T* raw_alloc(const std::size_t count)noexcept {
		if constexpr (sizeof(T) == 1) {
			if (count == 0)return nullptr;
		}
		else {
			constexpr std::size_t mask = (~static_cast<std::size_t>(0)) / sizeof(T);
			if (count == 0 || count > mask)return nullptr;
		}

		return 
			static_cast<T*>(details_::aligned_alloc_raw<alignof(T)>(sizeof(T) * count));
	}


	template<typename T>
	requires details_::raw_allocable<T>
	void raw_free(T* const p)noexcept {
		details_::aligned_free_raw<alignof(T)>(p);
	}



	template<typename T>
	concept reallocable =
		details_::raw_allocable<T> &&
		std::is_trivially_copyable_v<T> &&
		std::is_trivially_destructible_v<T> &&
		(alignof(T) <= details_::raw_alloc_basic_alignment);
}

#endif