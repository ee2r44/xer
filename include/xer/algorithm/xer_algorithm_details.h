#ifndef XER_ALGORITHM_DETAILS_H_
#define XER_ALGORITHM_DETAILS_H_

#include <type_traits>


namespace xer {
	namespace details_ {
		template<typename T>
		concept gt_judgable = requires(const T & a, const T & b) {
			{ a > b }noexcept;
			requires std::is_same_v<decltype(a > b), bool>;
		};

		template<typename T>
		concept gteq_judgable = requires(const T & a, const T & b) {
			{ a >= b }noexcept;
			requires std::is_same_v<decltype(a >= b), bool>;
		};

		template<typename T>
		concept ls_judgable = requires(const T & a, const T & b) {
			{ a < b }noexcept;
			requires std::is_same_v<decltype(a < b), bool>;
		};

		template<typename T>
		concept lseq_judgable = requires(const T & a, const T & b) {
			{ a <= b }noexcept;
			requires std::is_same_v<decltype(a <= b), bool>;
		};
	}
}

#endif