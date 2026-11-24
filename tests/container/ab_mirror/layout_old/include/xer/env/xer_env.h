#ifndef XER_ENV_H_
#define XER_ENV_H_

#include <cstddef>
#include "../macro/xer_macro.h"

namespace xer {
	namespace details_ {
		namespace env_ {
			enum support_enum :std::size_t {
				pad_ = 0,
#if XER_CPU(X86_64)||XER_CPU(X86_32)
				ssse3_support_,
				sse4_2_support_,
				popcnt_support_,
				bmi1_support_,
				bmi2_support_,
				lzcnt_support_,
				avx_support_,
				avx2_support_,
#endif
				COUNT_
			};


			struct alignas(XER_CACHE_LINE_SIZE) env {
				env(const env&) = delete;
				env(env&&) = delete;
				env& operator=(const env&) = delete;
				env& operator=(env&&) = delete;
				bool table_[COUNT_];

				inline ~env()noexcept = default;

				env()noexcept;

			};

			inline const env global_env_{};
		}
	}



	//禁止在其他全局构造使用
#if XER_CPU(X86_64)||XER_CPU(X86_32)

	inline bool is_ssse3_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::ssse3_support_];
	}
	inline bool is_sse4_2_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::sse4_2_support_];
	}
	inline bool is_popcnt_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::popcnt_support_];
	}
	inline bool is_bmi1_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::bmi1_support_];
	}
	inline bool is_bmi2_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::bmi2_support_];
	}
	inline bool is_lzcnt_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::lzcnt_support_];
	}
	// TZCNT 由 BMI1 提供
	inline bool is_tzcnt_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::bmi1_support_];
	}
	inline bool is_avx_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::avx_support_];
	}
	inline bool is_avx2_supported()noexcept {
		return details_::env_::global_env_.table_[details_::env_::avx2_support_];
	}
#endif
}


#endif