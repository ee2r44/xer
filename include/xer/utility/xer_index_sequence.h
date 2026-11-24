#ifndef XER_INDEX_SEQUENCE_H_
#define XER_INDEX_SEQUENCE_H_

#include <cstddef>
#include <type_traits>

#include "../macro/xer_macro.h"

namespace xer {
	namespace details_ {
		template<typename T, T...Ints>
			requires std::is_integral_v<T>
		struct integer_sequence {

			static constexpr std::size_t size()noexcept {
				return sizeof...(Ints);
			}
		};

		template<std::size_t...I>
		using index_sequence = integer_sequence<std::size_t, I...>;

#if XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE&&(XER_COMPILER(MSVC) || XER_COMPILER(CLANG))
		template<std::size_t N>
		using make_index_sequence = __make_integer_seq<integer_sequence, std::size_t, N>;
#elif XER_HAS_BUILTIN_MAKE_INDEX_SEQUENCE&&XER_COMPILER(GCC)
		template<std::size_t N>
		using make_index_sequence = index_sequence<__integer_pack(N)...>;
#else
		template<typename L, typename R>
		struct concat_index_seq;

		template<std::size_t...I0, std::size_t...I1>
		struct concat_index_seq<index_sequence<I0...>, index_sequence<I1...> > {
			using type = index_sequence<I0..., (sizeof...(I0) + I1)...>;
		};

		template<std::size_t N>
		struct temp_index_seq {
			using left = typename temp_index_seq<N / 2>::next;
			using right = typename temp_index_seq<N - N / 2>::next;
			using next = typename concat_index_seq<left, right>::type;
		};


		template<>
		struct temp_index_seq<1> {
			using next = index_sequence<0>;
		};

		template<>
		struct temp_index_seq<0> {
			using next = index_sequence<>;
		};


		template<std::size_t N>
		using make_index_sequence = temp_index_seq<N>::next;
#endif
	}


	template<std::size_t...I>
	using index_sequence = details_::index_sequence<I...>;

	template<std::size_t N>
	using make_index_sequence = details_::make_index_sequence<N>;
}

#endif