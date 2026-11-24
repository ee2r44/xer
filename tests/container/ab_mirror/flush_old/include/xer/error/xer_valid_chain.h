#ifndef XER_VALID_CHAIN_H_
#define XER_VALID_CHAIN_H_

#include <type_traits>

#include "../macro/xer_macro.h"
#include "../utility/xer_forward.h"

namespace xer {
	class valid_chain {
	private:
		bool judge;
	public:
		inline constexpr valid_chain()noexcept:
			judge(true){}


		inline constexpr valid_chain(const valid_chain&)noexcept = default;
		inline constexpr valid_chain(valid_chain&&)noexcept = default;
		inline constexpr valid_chain& operator=(const valid_chain&)noexcept = default;
		inline constexpr valid_chain& operator=(valid_chain&&)noexcept = default;
		inline constexpr ~valid_chain()noexcept = default;

		template<typename F,typename...Args>
		requires std::is_nothrow_invocable_r_v<bool,F,Args...>
		XER_FORCE_INLINE constexpr valid_chain& try_call(F&& f, Args&&...args)noexcept {
			judge = judge && static_cast<F&&>(f)(forward<Args>(args)...);
			return *this;
		}

		inline constexpr explicit operator bool()const noexcept {
			return judge;
		}


		inline constexpr valid_chain& operator =(const bool j)noexcept {
			judge = judge && j;
			return *this;
		}

	};
}

#endif