#ifndef XER_TUPLE_H_
#define XER_TUPLE_H_

#include <type_traits>
#include <cstddef>

#include "../utility/xer_forward.h"

namespace xer {
	namespace details_ {
		namespace tuple_ {
			template<std::size_t N,typename T,typename...Args>
			requires (N<sizeof...(Args)+1)
			struct type_n {
				using type = type_n<N - 1, Args...>::type;
			};

			template<typename T,typename...Args>
			struct type_n<0, T,Args...> {
				using type = T;
			};

			template<typename T,typename...Args>
			requires
			std::is_object_v<T>&&
			(std::is_object_v<Args>&&...)&&
			std::is_nothrow_destructible_v<T> &&
			(std::is_nothrow_destructible_v<Args>&&...)
			struct Pack {
				T data;
				Pack<Args...>next;

				constexpr Pack()noexcept
				requires std::is_nothrow_default_constructible_v<T> &&
				(std::is_nothrow_default_constructible_v<Args> && ...) = default;

				template<typename U,typename...Us>
				requires
				std::is_nothrow_constructible_v<T,U>&&
				(sizeof...(Us) == sizeof...(Args)) &&
				(std::is_nothrow_constructible_v<Args, Us>&&...)
				constexpr Pack(U&&u,Us&&...us)noexcept
				requires (sizeof...(Us)>0):
					data(::xer::forward<U>(u)),
					next(::xer::forward<Us>(us)...){}

				template<std::size_t N>
				requires
				(N< sizeof...(Args)+1)
				constexpr type_n<N,T,Args...>::type& get()noexcept {
					if constexpr (N == 0)return data;
					else return next.template get<N - 1>();
				}

				template<std::size_t N>
					requires
				(N < sizeof...(Args) + 1)
				constexpr const type_n<N, T, Args...>::type& get()const noexcept {
					if constexpr (N == 0)return data;
					else return next.template get<N - 1>();
				}
			};

			template<typename T>
			requires
			std::is_object_v<T>&&
			std::is_nothrow_destructible_v<T>
			struct Pack<T> {
				T data;

				constexpr Pack()noexcept
				requires std::is_nothrow_default_constructible_v <T> = default;

				template<typename U>
				requires
				std::is_nothrow_constructible_v<T,U>
				constexpr Pack(U&&u)noexcept:
					data(::xer::forward<U>(u)){}

				template<std::size_t N>
				requires (N==0)
				constexpr T& get()noexcept {
					return data;
				}

				template<std::size_t N>
					requires (N == 0)
				constexpr const T& get()const noexcept {
					return data;
				}
			};
		}
	}


	template<typename...Args>
	requires
	(std::is_object_v<Args>&&...)&&
	(std::is_nothrow_destructible_v<Args>&&...)
	struct tuple {
		template<std::size_t N>
		requires (N<sizeof...(Args))
		using type = details_::tuple_::type_n<N, Args...>::type;


		details_::tuple_::Pack<Args...> pack_;

		constexpr tuple() noexcept
		requires (std::is_nothrow_default_constructible_v<Args>&&...) = default;

		template<typename...Us>
		requires
		(sizeof...(Us)==sizeof...(Args))&&
		(std::is_nothrow_constructible_v<Args,Us>&&...)
		constexpr tuple(Us&&...us)noexcept
		requires (sizeof...(Us)>0) :
			pack_(::xer::forward<Us>(us)...){}



		template<std::size_t N>
		requires (N<sizeof...(Args))
		constexpr type<N>& get()noexcept {
			return pack_.template get<N>();
		}


		template<std::size_t N>
			requires (N < sizeof...(Args))
		constexpr const type<N>& get()const noexcept {
			return pack_.template get<N>();
		}
	};


	template<>
	struct tuple<> {
		constexpr tuple() noexcept = default;
	};
}

#endif