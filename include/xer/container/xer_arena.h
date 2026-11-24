#ifndef XER_ARENA_H_
#define XER_ARENA_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "../bit/xer_has_single_bit.h"

#include "../algorithm/xer_max.h"
#include "../numeric/xer_add_overflow.h"
#include "../utility/xer_move.h"
#include "xer_vector.h"

namespace xer {
	class arena {
	private:
		arena(const arena&) = delete;
		arena& operator=(const arena&) = delete;

		static constexpr std::size_t DFSize = (static_cast<std::size_t>(1) << 11) + (static_cast<std::size_t>(1) << 10);

		vector<std::uint8_t*>pages_;
		std::size_t pindex_;
		std::size_t capacity_;
		std::size_t needle_;


		//assume ps >0
		inline bool increase_page(const std::size_t cap)noexcept {
			if (!pages_.reserve(pages_.size() + 1))XER_UNLIKELY return false;

			std::uint8_t* const p = static_cast<std::uint8_t*>(std::malloc(cap));
			if (p == nullptr)return false;

			pages_.push_back_unchecked(p);

			pindex_ = pages_.size() - 1;
			capacity_ = cap;
			needle_ = 0;
			return true;
		}

		//assume align has single bit && size >0
		inline std::uint8_t* alloc_imply(const std::size_t align,const std::size_t size)noexcept {
			using U = std::conditional_t<(sizeof(std::size_t) > sizeof(std::uintptr_t)), std::size_t, std::uintptr_t>;
			const std::size_t lmask = align - 1;
			const U hmask = ~static_cast<U>(lmask);
			if (add_overflow(size, lmask))return nullptr;

			const std::size_t fine_size = size + lmask;
			std::size_t D = max(DFSize,capacity_);
			if ((capacity_ == 0 || capacity_ - needle_ < fine_size) &&
				(!increase_page(max(D + (D >> 1), fine_size)))
				)return nullptr;

			const std::uintptr_t p0 = reinterpret_cast<std::uintptr_t>(pages_[pindex_] + needle_);
			const std::uintptr_t p1 = (p0 + lmask) & hmask;
			needle_ += p1 - p0 + size;
			return reinterpret_cast<std::uint8_t*>(p1);
		}

		inline void destroy()noexcept {
			for (const auto p : pages_) {
				std::free(p);
			}
			pages_.clear();
		}

	public:
		inline arena()noexcept:
			pages_(),
			pindex_(0),
			capacity_(0),
			needle_(0){}

		inline arena(arena&& a)noexcept :
			pages_(::xer::move(a.pages_)),
			pindex_(a.pindex_),
			capacity_(a.capacity_),
			needle_(a.needle_) {
			a.pindex_ = 0;
			a.capacity_ = 0;
			a.needle_ = 0;
		}

		inline arena& operator=(arena&& a)noexcept {
			if (this != &a) {
				destroy();
				pages_ = ::xer::move(a.pages_);
				pindex_ = a.pindex_;
				capacity_ = a.capacity_;
				needle_ = a.needle_;

				a.pindex_ = 0;
				a.capacity_ = 0;
				a.needle_ = 0;
			}

			return *this;
		}

		inline ~arena()noexcept {
			destroy();
		}

		inline void reset()noexcept {
			destroy();
			pindex_ = 0;
			capacity_ = 0;
			needle_ = 0;
		}

		template<typename T>
		requires std::is_object_v<T>
		T* alloc(const std::size_t count = 1)noexcept {
			if (count == 0)return nullptr;
			if constexpr (sizeof(T) > 1) {
				constexpr std::size_t GT = (~static_cast<std::size_t>(0)) / sizeof(T);
				if (count > GT)return nullptr;
			}

			return reinterpret_cast<T*>(alloc_imply(alignof(T), sizeof(T) * count));
		}

		inline void* align_alloc(const std::size_t align, const std::size_t size)noexcept {
			return
				(has_single_bit(align)&&size>0) ?
				alloc_imply(align, size) :
				nullptr;
		}
	};
}


#endif