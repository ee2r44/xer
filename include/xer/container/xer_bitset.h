#ifndef XER_BITSET_H_
#define XER_BITSET_H_

#include <cstring>
#include <type_traits>
#include <cstddef>
#include <cstdint>


#include "../macro/xer_macro.h"
#include "xer_array.h"

namespace xer {

	template<std::size_t bit_count>
		requires (bit_count > 0)
	class bitset {
	private:


		static constexpr bool is_env64 = XER_ARCH(64);

		static constexpr std::size_t unit_count32 = (bit_count + 31) >> 5;
		static constexpr std::size_t unit_count64 = (bit_count + 63) >> 6;
		static constexpr std::size_t unit_count = is_env64 ? unit_count64 : unit_count32;
		using unit_t = std::conditional_t<is_env64, std::uint64_t, std::uint32_t>;

		static constexpr std::size_t bit_gate_ = sizeof(unit_t) << 3;
		static constexpr std::size_t bit_needle_ = is_env64 ? 6 : 5;
		static constexpr std::size_t bit_mask_ = bit_gate_ - 1;



		using Array = array<unit_t, unit_count>;

		using table_type32 = std::conditional_t<bit_count <= 8, std::uint8_t, std::conditional_t<bit_count <= 16, std::uint16_t, std::conditional_t<bit_count <= 32, std::uint32_t, Array > > >;
		using table_type64 = std::conditional_t<bit_count <= 8, std::uint8_t, std::conditional_t<bit_count <= 16, std::uint16_t, std::conditional_t<bit_count <= 32, std::uint32_t, std::conditional_t<bit_count <= 64, std::uint64_t, Array > > > >;
		using table_type = std::conditional_t<is_env64, table_type64, table_type32>;


		table_type table;

	public:
		constexpr bitset()noexcept = default;
		constexpr bitset(const bitset&)noexcept = default;
		constexpr bitset(bitset&&)noexcept = default;
		constexpr bitset& operator =(const bitset&)noexcept = default;
		constexpr bitset& operator =(bitset&&)noexcept = default;
		constexpr ~bitset()noexcept = default;



		constexpr std::size_t size()const noexcept { return bit_count; }


		constexpr void set_unchecked(const std::size_t index, const bool value)noexcept
			requires (bit_count <= bit_gate_) {
			const table_type mask = ~(static_cast<table_type>(1) << index);
			const table_type needle = static_cast<table_type>(value) << index;

			table = (table & mask) | needle;
		}
		constexpr void set_unchecked(const std::size_t index, const bool value)noexcept
			requires (bit_count > bit_gate_) {
			const std::size_t ui = index >> bit_needle_;

			const std::size_t shift = index & bit_mask_;
			const unit_t mask = ~(static_cast<unit_t>(1) << shift);
			const unit_t needle = static_cast<unit_t>(value) << shift;

			table[ui] = (table[ui] & mask) | needle;
		}

		constexpr void set_unchecked(const std::size_t index)noexcept
			requires (bit_count <= bit_gate_) {
			const table_type needle = static_cast<table_type>(1) << index;
			table |= needle;
		}
		constexpr void set_unchecked(const std::size_t index)noexcept
			requires (bit_count > bit_gate_) {
			const std::size_t ui = index >> bit_needle_;
			const unit_t needle = static_cast<unit_t>(1) << (index & bit_mask_);
			table[ui] |= needle;
		}


		constexpr void reset_unchecked(const std::size_t index)noexcept
			requires(bit_count <= bit_gate_) {
			const table_type mask = ~(static_cast<table_type>(1) << index);
			table &= mask;
		}
		constexpr void reset_unchecked(const std::size_t index)noexcept
			requires(bit_count > bit_gate_) {
			const std::size_t ui = index >> bit_needle_;
			const unit_t mask = ~(static_cast<unit_t>(1) << (index & bit_mask_));
			table[ui] &= mask;
		}


		constexpr bool get_unchecked(const std::size_t index)const noexcept
			requires (bit_count <= bit_gate_) {
			return static_cast<bool>((table >> index) & static_cast<table_type>(1));
		}
		constexpr bool get_unchecked(const std::size_t index)const noexcept
			requires (bit_count > bit_gate_) {
			const std::size_t ui = index >> bit_needle_;

			return static_cast<bool>((table[ui] >> (index & bit_mask_)) & static_cast<unit_t>(1));
		}


		constexpr void flip_unchecked(const std::size_t index)noexcept
			requires (bit_count <= bit_gate_) {
			const table_type needle = static_cast<table_type>(1) << index;

			table ^= needle;
		}
		constexpr void flip_unchecked(const std::size_t index)noexcept
			requires (bit_count > bit_gate_) {
			const std::size_t ui = index >> bit_needle_;
			const unit_t needle = static_cast<unit_t>(1) << (index & bit_mask_);

			table[ui] ^= needle;
		}


		constexpr void set_all()noexcept
			requires(bit_count <= bit_gate_) {
			constexpr table_type mask = ~static_cast<table_type>(0);
			table = mask;
		}
		constexpr void set_all()noexcept
			requires(bit_count > bit_gate_) {
			constexpr unit_t mask = ~static_cast<unit_t>(0);

			if consteval {
				for (auto& a : table)a = mask;
			}
			else {
				constexpr std::size_t mem_size = unit_count * sizeof(unit_t);
				std::memset(table.data(), 0xFF, mem_size);
			}
		}

		constexpr void set_all(const bool value)noexcept {
			if (value)set_all();
			else reset_all();
		}

		constexpr void flip_all()noexcept
			requires(bit_count <= bit_gate_) {
			constexpr table_type mask = ~static_cast<table_type>(0);
			table ^= mask;
		}
		constexpr void flip_all()noexcept
			requires(bit_count > bit_gate_) {
			constexpr unit_t mask = ~static_cast<unit_t>(0);

			for (auto& a : table)a ^= mask;
		}

		constexpr void reset_all()noexcept
			requires(bit_count <= bit_gate_) {
			table = 0;
		}
		constexpr void reset_all()noexcept
			requires(bit_count > bit_gate_) {
			if consteval {
				for (auto& a : table)a = 0;
			}
			else {
				static constexpr std::size_t mem_size = unit_count * sizeof(unit_t);
				std::memset(table.data(), 0, mem_size);
			}
		}

	};


	static_assert(
		std::is_trivial_v<bitset<7> >&&
		std::is_trivial_v<bitset<8> >&&
		std::is_trivial_v<bitset<15> >&&
		std::is_trivial_v<bitset<16> >&&
		std::is_trivial_v<bitset<31> >&&
		std::is_trivial_v<bitset<32> >&&
		std::is_trivial_v<bitset<63> >&&
		std::is_trivial_v<bitset<64> >&&
		std::is_trivial_v<bitset<127> >&&
		std::is_trivial_v<bitset<128> >&&
		std::is_trivial_v<bitset<255> >&&
		std::is_trivial_v<bitset<256> >,
		"bitset must be trivial-type");
}


#endif