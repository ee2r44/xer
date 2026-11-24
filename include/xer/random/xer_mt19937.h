#ifndef XER_MT19937_H_
#define XER_MT19937_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "../algorithm/xer_max.h"
#include "../macro/xer_macro.h"
#include "../error/xer_valid_chain.h"

namespace xer {
	class mt19937 {
	private:
		mt19937(const mt19937&) = delete;
		mt19937& operator=(const mt19937&) = delete;

		static inline constexpr std::uint32_t W = 32;
		static inline constexpr std::uint32_t N = 624;
		static inline constexpr std::uint32_t M = 397;
		static inline constexpr std::uint32_t R = 31;
		static inline constexpr std::uint32_t W_SUB_R = W - R;
		static inline constexpr std::uint32_t A = static_cast<std::uint32_t>(0x9908B0DF);
		static inline constexpr std::uint32_t U = 11;
		static inline constexpr std::uint32_t S = 7;
		static inline constexpr std::uint32_t T = 15;
		static inline constexpr std::uint32_t L = 18;
		static inline constexpr std::uint32_t B = static_cast<std::uint32_t>(0x9D2C5680);
		static inline constexpr std::uint32_t C = static_cast<std::uint32_t>(0xEFC60000);

		static inline constexpr std::uint32_t LOWER_MASK = (static_cast<std::uint32_t>(1) << R) - 1;
		static inline constexpr std::uint32_t UPPER_MASK = ~LOWER_MASK;

	private:
		std::uint32_t* mt_;
		std::uint32_t index_;

		inline bool init_0(const std::uint32_t seed)noexcept { 
			if (mt_ == nullptr && 
				(mt_ = static_cast<std::uint32_t*>(std::malloc(sizeof(std::uint32_t) * N))) == nullptr)
				return false;

			mt_[0] = seed;
			for (std::size_t i = 1; i < N; i++) {
				const std::uint32_t x = mt_[i - 1];
				mt_[i] = static_cast<std::uint32_t>(1812433253) * (x ^ (x >> 30)) + i;
			}

			index_ = N;
			return true;
		}

		inline bool init_1(const std::uint32_t* const XER_RESTRICT p, const std::size_t count)noexcept {
			constexpr std::size_t NS = N - 1;
			if (count == 0 || (!init_0(static_cast<std::uint32_t>(19650218))))return false;

			std::size_t i = 1;
			std::size_t j = 0;

			const std::size_t b = ::xer::max(count, static_cast<std::size_t>(N));
			for (std::size_t k = 0; k < b; k++) {
				const std::uint32_t x = mt_[i - 1];
				mt_[i] ^= (static_cast<std::uint32_t>(1664525) * (x ^ (x >> 30)));
				mt_[i] += p[j] + j;

				if (i == NS) {
					mt_[0] = mt_[NS];
					i = 1;
				}
				else {
					i++;
				}
				j = (j + 1) % count;
			}

			for (std::size_t k = 0; k < NS; k++) {
				const std::uint32_t x = mt_[i - 1];
				mt_[i] ^= static_cast<std::uint32_t>(1566083941) * (x ^ (x >> 30));
				mt_[i] -= i;


				if (i == NS) {
					mt_[0] = mt_[NS];
					i = 1;
				}
				else {
					i++;
				}
			}

			mt_[0] = UPPER_MASK;

			index_ = N;
			return true;
		}


		inline mt19937()noexcept:
			mt_(nullptr),
			index_(N){}
	public:
		inline mt19937(valid_chain& v)noexcept:
		mt19937(){
			v = v && init_0(5489);
		}

		inline mt19937(valid_chain& v, const std::uint32_t seed)noexcept :
			mt19937() {
			v = v && init_0(seed);
		}


		inline mt19937(valid_chain& v, const std::uint32_t* const XER_RESTRICT p, const std::size_t count)noexcept:
			mt19937(){
			v = v && init_1(p, count);
		}


		inline mt19937(mt19937&& m)noexcept :
			mt_(m.mt_),
			index_(m.index_) {
			m.mt_ = nullptr;
			m.index_ = N;
		}

		inline mt19937& operator=(mt19937&& m)noexcept {
			if (this != &m) {
				std::free(mt_);
				mt_ = m.mt_;
				index_ = m.index_;
				m.mt_ = nullptr;
				m.index_ = N;
			}
			return *this;
		}

		inline ~mt19937()noexcept {
			std::free(mt_);
		}

		inline bool reset()noexcept {
			return init_0(5489);
		}

		inline bool reset(const std::uint32_t seed)noexcept {
			return init_0(seed);
		}

		inline bool reset(const std::uint32_t* const XER_RESTRICT p, const std::size_t count)noexcept {
			return init_1(p, count);
		}

		inline std::uint32_t operator()()noexcept {
			constexpr std::uint32_t NS = N - 1;
			constexpr std::uint32_t NSM = N - M;

			if (index_ > NS) {
				static constexpr std::uint32_t arr[2]{ 0,A };

				std::size_t i = 0;
				for (; i < NSM; i++) {
					std::uint32_t y = (mt_[i] & UPPER_MASK) | (mt_[i + 1] & LOWER_MASK);
					const std::uint32_t judge = y & 1;
					y >>= 1;
					y ^= arr[judge];


					mt_[i] = y ^ mt_[i + M];
				}

				for (; i < NS; i++) {
					std::uint32_t y = (mt_[i] & UPPER_MASK) | (mt_[i + 1] & LOWER_MASK);
					const std::uint32_t judge = y & 1;
					y >>= 1;
					y ^= arr[judge];


					mt_[i] = y ^ mt_[i - NSM];
				}

				std::uint32_t y = (mt_[NS] & UPPER_MASK) | (mt_[0] & LOWER_MASK);
				const std::uint32_t judge = y & 1;

				y >>= 1;
				y ^= arr[judge];


				mt_[i] = y ^ mt_[M - 1];


				index_ = 0;

			}

			std::uint32_t y = mt_[index_];
			index_++;

			y ^= y >> U;
			y ^= (y << S) & B;
			y ^= (y << T) & C;
			y ^= y >> L;

			return y;
		}
	};
}

#endif