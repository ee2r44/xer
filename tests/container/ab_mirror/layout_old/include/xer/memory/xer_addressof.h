#ifndef XER_ADDRESSOF_H_
#define XER_ADDRESSOF_H_

#include "../macro/xer_macro.h"


namespace xer {
#if XER_HAS_BUILTIN_ADDRESSOF
	template<typename T>
	constexpr T* addressof(T& t)noexcept {
		return __builtin_addressof(t);
	}
#else
	template<typename T>
	T* addressof(T& t)noexcept {
		return reinterpret_cast<T*>(&const_cast<char&>(reinterpret_cast<const volatile char&>(t)));
	}
#endif


	template<typename T>
	T* addressof(T&&)noexcept = delete;
}

#endif