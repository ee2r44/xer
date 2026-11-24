#ifndef XER_UNORDERED_FLAT_TABLE_H_
#define XER_UNORDERED_FLAT_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "../macro/xer_macro.h"
#include "../bit/xer_countr_zero.h"
#include "../hash/xer_hash.h"
#include "../memory/xer_raw_alloc.h"
#include "../memory/xer_construct.h"
#include "../memory/xer_addressof.h"
#include "../utility/xer_move.h"
#include "../utility/xer_forward.h"
#include "../algorithm/xer_max.h"
#include "../algorithm/xer_min.h"
#include "xer_pair.h"
#include "../error/xer_ret.h"

#if XER_SSE2_AVAILABLE
#	include <immintrin.h>
#endif


//不实现哨兵语义
namespace xer {

	namespace details_ {
		namespace unordered_flat_table_ {
			template<typename Key>
			concept is_valid_key = requires(const Key & k0, const Key & k1) {
				{ k0 == k1 }noexcept;
				requires std::is_same_v<decltype(k0 == k1), bool>;
				requires std::is_nothrow_destructible_v<Key>;
			};

			template<typename Value>
			concept is_valid_value =
				std::is_nothrow_destructible_v<Value>;


			struct alignas(16) Group {
				std::uint8_t ctrls[15];
				std::uint8_t ofw;
			};
#if XER_SSE2_AVAILABLE
			struct M128i {
				__m128i h;

				inline M128i(const Group* const XER_RESTRICT p)noexcept :
					h(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))) {
				}

				inline std::uint32_t match_empty()const noexcept {
					const __m128i j{};

					return _mm_movemask_epi8(_mm_cmpeq_epi8(h, j))&0x7FFF;
				}

				inline std::uint32_t match_have_unit()const noexcept {
					const __m128i j{};
					return (~_mm_movemask_epi8(_mm_cmpeq_epi8(h, j))) & 0x7FFF;
				}

				inline std::uint32_t match(const std::uint8_t k)const noexcept {
					const __m128i j = _mm_set1_epi8(static_cast<char>(k));

					return _mm_movemask_epi8(_mm_cmpeq_epi8(h, j)) & 0x7FFF;
				}
			};
#endif


		}


		template<typename Key,typename Value,bool has_value,typename HS>
		requires 
		hashable<HS, Key>&&
		unordered_flat_table_::is_valid_key<Key>&&
		unordered_flat_table_::is_valid_value<Value>
		class unordered_flat_table {
		private:
			unordered_flat_table(const unordered_flat_table&) = delete;
			unordered_flat_table& operator=(const unordered_flat_table&) = delete;



			using Group = unordered_flat_table_::Group;
			using Unit = std::conditional_t<has_value, pair<Key, Value>, Key>;

			/*
			struct Group {
				std::uint8_t ctrls[15];
				std::uint8_t ofw;
				Unit units[15];
			};
			*/

#if XER_SSE2_AVAILABLE
			using M128i = unordered_flat_table_::M128i;
#endif

		
		private:


			static std::uint8_t fine_label(const std::size_t hash_r)noexcept {
				const std::uint32_t temp = static_cast<std::uint8_t>(hash_r);
				return 
					static_cast<std::uint8_t>(temp + static_cast<std::uint32_t>(temp == 0));
			}

			static std::size_t gtou_index(const std::size_t i, const std::uint32_t j)noexcept {
				return i * 15 + j;
			}

			static void flush_groups(Group* const XER_RESTRICT pg, const std::size_t g_count)noexcept {
				std::memset(pg, 0, sizeof(Group) * g_count);
			}

			static const Key& get_unit_key(const Unit& u)noexcept {
				if constexpr (has_value) {
					return u.first;
				}
				else {
					return u;
				}
			}

			static std::size_t get_init_gate(const std::size_t group_count)noexcept {
				const std::size_t temp = group_count * 15;

				return temp - (temp >> 3);
			}

			//assume cap has single bit
			static Group* try_get_enough_memory(const std::size_t cap)noexcept {
				constexpr std::size_t pad =
					alignof(Group) >= alignof(Unit) ?
					0 :
					alignof(Unit) - alignof(Group);
				constexpr std::size_t M0 = sizeof(Group) + sizeof(Unit) * 15;
				constexpr std::size_t G = min(
					static_cast<std::size_t>(((~static_cast<std::size_t>(0)) >> 8) + 1),
					static_cast<std::size_t>(((~static_cast<std::size_t>(0)) - pad) / M0)
				);
				return 
					cap > G ? 
					nullptr : 
					static_cast<Group*>(details_::aligned_alloc_raw<alignof(Group)>(M0 * cap + pad));
			}

			static Unit* cacul_punit(Group* const XER_RESTRICT pgroup,const std::size_t group_count)noexcept {
				if constexpr (alignof(Unit) <= alignof(Group)) {
					return reinterpret_cast<Unit*>(pgroup+group_count);
				}
				else {
					constexpr std::uintptr_t lmask = alignof(Unit) - 1;
					constexpr std::uintptr_t hmask = ~lmask;

					return reinterpret_cast<Unit*>(
						(reinterpret_cast<std::uintptr_t>(pgroup +group_count) + lmask) & hmask);
				}
			}

			std::size_t get_unit_hash(const Unit& u)const noexcept {
				if constexpr (has_value) {
					return h_(u.first);
				}
				else {
					return h_(u);
				}
			}

			//assume pgroup == nullptr
			bool capacity_init()noexcept {

				pgroup_ = try_get_enough_memory(1);
				if (pgroup_ == nullptr)XER_UNLIKELY return false;
				flush_groups(pgroup_, 1);
				punit_ = cacul_punit(pgroup_, 1);

				group_count_ = 1;
				group_mask_ = 0;

				size_ = 0;
				gate_ = get_init_gate(1);

				return true;
			}

			//assume new_cap has single bit && new_cap is enough for size_
			void change_capacity(Group*const XER_RESTRICT new_pgroup,const std::size_t new_cap)noexcept {
				Unit* const new_punit = cacul_punit(new_pgroup, new_cap);

				flush_groups(new_pgroup, new_cap);
				const std::size_t new_group_mask = new_cap - 1;

				const std::size_t i0End = group_count_;
				for (std::size_t i0 = 0; i0 < i0End; i0++) {
#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge0 = M128i{ pgroup_ + i0 }.match_have_unit();
						judge0 != 0;
						judge0 &= judge0 - 1) {
						Unit& u = punit_[gtou_index(i0, countr_zero(judge0))];

						const std::size_t hash_r = get_unit_hash(u);

						std::size_t index = (hash_r >> 8)&new_group_mask;
						const std::uint8_t label = fine_label(hash_r);

						std::size_t step = 0;

						while (true) {
							Group* const pg1 = new_pgroup + index;
							const std::uint32_t judge1 = M128i{ pg1 }.match_empty();
							if (judge1 != 0) {
								const std::uint32_t j1 = countr_zero(judge1);

								pg1->ctrls[j1] = label;

								transfer_construct_at(
									new_punit + gtou_index(index,j1),
									u);
								break;
							}
							else {
								new_pgroup[index].ofw |= 1 << (label & 0x7);
								step++;
								index = (index + step) & new_group_mask;
							}
						}

					}

#else
					const std::uint8_t* const pc0 = pgroup_[i0].ctrls;
					for (std::size_t j0 = 0; j0 < 15; j0++) {
						if (pc0[j0] != 0) {
							Unit& u = punit_[gtou_index(i0, j0)];

							const std::size_t hash_r = get_unit_hash(u);

							std::size_t index = (hash_r >> 8)&new_group_mask;
							const std::uint8_t label = fine_label(hash_r);

							std::size_t step = 0;

							while (true) {
								std::uint8_t* const pc1 = new_pgroup[index].ctrls;
								for (std::uint32_t j1 = 0; j1 < 15; j1++) {
									if (pc1[j1] == 0) {
										pc1[j1] = label;
										transfer_construct_at(
											new_punit + gtou_index(index, j1),
											u);
										goto tsf_end;
									}
								}


								new_pgroup[index].ofw |= 1 << (label & 7);
								step++;
								index = (index + step) & new_group_mask;
							}

						tsf_end:;
						}
					}
#endif
				}


				details_::aligned_free_raw<alignof(Group)>(pgroup_);

				pgroup_ = new_pgroup;
				punit_ = new_punit;

				group_count_ = new_cap;
				group_mask_ = new_group_mask;
				gate_ = get_init_gate(new_cap);
			}



			/*
			* assume have free space
			* if success, will write the label byte
			*/
			Unit* try_insert_unckecked(const Key& k)const noexcept {
				const std::size_t hash_r = h_(k);
				std::size_t index = (hash_r >> 8)&group_mask_;
				const std::uint8_t label = fine_label(hash_r);

				bool find_e = false;
				std::uint32_t e_j;
				std::size_t e_index;

				std::size_t step = 0;
				while (step < group_count_) {
					const Group* const pg = pgroup_ + index;
#if XER_SSE2_AVAILABLE
					const M128i mi{ pg };

					for (std::uint32_t judge = mi.match(label);
						judge != 0;
						judge &= judge - 1) {
						if (k == get_unit_key(punit_[gtou_index(index, countr_zero(judge))]))
							return nullptr;
					}

					if (!find_e) {
						const std::uint32_t judge = mi.match_empty();
						if (judge != 0) {
							find_e = true;
							e_j = countr_zero(judge);
							e_index = index;
						}
					}
#else

					const std::uint8_t* const pc = pg->ctrls;
					for (std::uint32_t i = 0; i < 15; i++) {
						if (pc[i] == label && k == get_unit_key(punit_[gtou_index(index, i)]))
							return nullptr;
					}


					if (!find_e) {
						for (std::uint32_t i = 0; i < 15; i++) {
							if (pc[i] == 0) {
								find_e = true;
								e_j = i;
								e_index = index;
								break;
							}
						}
					}
#endif

					if ((pg->ofw & (1 << (label & 0x7))) == 0)break;
					step++;
					index = (index + step) & group_mask_;
				}

				if (find_e) {
					pgroup_[e_index].ctrls[e_j] = label;
					return punit_ + gtou_index(e_index, e_j);
				}
				else {//!find_e
					Group* pg = pgroup_ + index;

					while (true) {
						pg->ofw |= 1 << (label & 0x7);
						step++;
						index = (index + step) & group_mask_;
						pg = pgroup_ + index;

#if XER_SSE2_AVAILABLE
						const std::uint32_t judge = M128i{ pg }.match_empty();
						if (judge != 0) {
							const std::uint32_t i = countr_zero(judge);
							pg->ctrls[i] = label;

							return punit_ + gtou_index(index, i);
						}
#else
						std::uint8_t* const pc = pg->ctrls;
						for (std::uint32_t i = 0; i < 15; i++) {
							if (pc[i] == 0) {
								pc[i] = label;

								return punit_ + gtou_index(index, i);
							}
						}
#endif
					}
				}
			}

			using FindPtr_t = std::conditional_t<has_value, Value*, Key*>;
			using FindPtrConst_t = std::conditional_t<has_value, const Value*, const Key*>;

			FindPtr_t find_impl(const Key& k)const noexcept {
				const std::size_t hash_r = h_(k);
				std::size_t index = (hash_r >> 8) & group_mask_;
				const std::uint8_t label = fine_label(hash_r);

				std::size_t step = 0;
				while (step < group_count_) {
					Group* const pg = pgroup_ + index;
#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge = M128i{ pg }.match(label);
						judge != 0;
						judge &= judge - 1) {
						Unit* const pu = punit_+ gtou_index(index ,countr_zero(judge));
						if (k == get_unit_key(*pu)) {
							if constexpr (has_value) {
								return addressof(pu->second);
							}
							else {
								return pu;
							}
						}
					}
#else
					std::uint8_t* const pc = pg->ctrls;
					for (std::uint32_t i = 0; i < 15; i++) {
						if (pc[i] == label) {
							Unit* const pu = punit_ + gtou_index(index, i);
							if (k == get_unit_key(*pu)) {
								if constexpr (has_value) {
									return addressof(pu->second);
								}
								else {
									return pu;
								}
							}
						}
					}
#endif
					if ((pg->ofw & (1 << (label & 0x7))) == 0)break;

					step++;
					index = (index + step) & group_mask_;
				}

				return nullptr;
			}


			void destroy_units_and_free()noexcept {
				for (std::size_t i = 0; i < group_count_; i++) {
					const Group* const pg = pgroup_ + i;

#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge = M128i{ pg }.match_have_unit();
						judge != 0;
						judge &= judge - 1) {
						destroy_at(punit_ + gtou_index(i, countr_zero(judge)));
					}
#else
					const std::uint8_t* const pc = pg->ctrls;

					for (std::uint32_t j = 0; j < 15; j++) {
						if (pc[j] != 0) {
							destroy_at(punit_ + gtou_index(i, j));
						}
					}
#endif

				}

				aligned_free_raw<alignof(Group)>(pgroup_);
			}

			void to_void()noexcept {
				pgroup_ = nullptr;
				punit_ = nullptr;
				group_count_ = 0;
				group_mask_ = 0;
				size_ = 0;
				gate_ = 0;
			}

			//assume size_>=gate_
			ret flush_unchecked()noexcept {
				constexpr std::size_t GT = (~std::size_t{ 0 }) >> 1;
				std::size_t need = max(group_count_ >> 1, static_cast<std::size_t>(1));
				while (get_init_gate(need) <= size_) {
					if (need > GT)XER_UNLIKELY return ret::alloc_failure;
					need <<= 1;
				}
				Group* const new_pgroup = try_get_enough_memory(need);
				if (new_pgroup == nullptr)XER_UNLIKELY return ret::alloc_failure;
				change_capacity(new_pgroup, need);
				return ret::success;
			}


			template<bool is_const,std::uint32_t JG,typename F>
			requires
			(JG==0&&std::is_nothrow_invocable_r_v<void, F,const Key&>) ||
			(JG==1&&has_value && std::is_nothrow_invocable_r_v<void, F,std::conditional_t<is_const,const Value&,Value&> >) ||
			(JG==2&&has_value && std::is_nothrow_invocable_r_v<void, F, const Key&, std::conditional_t<is_const, const Value&, Value&> >)
			void for_each_impl(F && f)const noexcept {

				for (std::size_t i = 0; i < group_count_; i++) {
					const Group* const pg = pgroup_ + i;

#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge = M128i{ pg }.match_have_unit();
						judge != 0;
						judge &= judge - 1) {
						const std::uint32_t j = countr_zero(judge);
						Unit* const pu = punit_ + gtou_index(i, j);

						if constexpr (JG == 0) {
							static_cast<F&&>(f)(get_unit_key(*pu));
						}
						else if constexpr (JG == 1) {
							static_cast<F&&>(f)(pu->second);
						}
						else {
							static_cast<F&&>(f)(pu->first, pu->second);
						}
					}
#else
					std::uint8_t* const pc = pg->ctrls;

					for (std::uint32_t j = 0; j < 15; j++) {
						const std::uint8_t label = pc[j];
						if (label != 0) {
							Unit* const pu = punit_ + gtou_index(i, j);

							if constexpr (JG == 0) {
								static_cast<F&&>(f)(get_unit_key(*pu));
							}
							else if constexpr (JG == 1) {
								static_cast<F&&>(f)(pu->second);
							}
							else {
								static_cast<F&&>(f)(pu->first, pu->second);
							}
						}
					}
#endif

				}
			}

			template<std::uint32_t JG, typename F>
				requires
			(JG == 0 && std::is_nothrow_invocable_r_v<bool, F, const Key&>) ||
				(JG == 1 && has_value && std::is_nothrow_invocable_r_v<bool, F, const Value&>) ||
				(JG == 2 && has_value && std::is_nothrow_invocable_r_v<bool, F, const Key&, const Value&>)
				void erase_if_impl(F && f)noexcept {

				for (std::size_t i = 0; i < group_count_; i++) {
					Group* const pg = pgroup_ + i;

#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge = M128i{ pg }.match_have_unit();
						judge != 0;
						judge &= judge - 1) {
						const std::uint32_t j = countr_zero(judge);
						Unit* const pu = punit_ + gtou_index(i, j);

						if constexpr (JG == 0) {
							if (!static_cast<F&&>(f)(get_unit_key(*pu))) continue;
						}
						else if constexpr (JG == 1) {
							if (!static_cast<F&&>(f)(pu->second)) continue;
						}
						else {
							if (!static_cast<F&&>(f)(pu->first, pu->second)) continue;
						}

						destroy_at(pu);
						size_--;
						if (pg->ofw & static_cast<std::uint8_t>(1 << ((pg->ctrls[j]) & 0x7)))gate_--;
						pg->ctrls[j] = 0;
					}
#else
					std::uint8_t* const pc = pg->ctrls;

					for (std::uint32_t j = 0; j < 15; j++) {
						const std::uint8_t label = pc[j];
						if (label != 0) {
							Unit* const pu = punit_ + gtou_index(i, j);

							if constexpr (JG == 0) {
								if (!static_cast<F&&>(f)(get_unit_key(*pu))) continue;
							}
							else if constexpr (JG == 1) {
								if (!static_cast<F&&>(f)(pu->second)) continue;
							}
							else {
								if (!static_cast<F&&>(f)(pu->first, pu->second)) continue;
							}

							destroy_at(pu);
							size_--;
							if (pg->ofw & static_cast<std::uint8_t>(1 << (label & 0x7)))gate_--;
							pc[j] = 0;
						}
					}
#endif

				}
			}


			HS h_;

			Group* pgroup_;
			Unit* punit_;
			std::size_t group_count_;
			std::size_t group_mask_;

			std::size_t size_;
			std::size_t gate_;
		public:
			unordered_flat_table()noexcept:
				h_(),
				pgroup_(nullptr),
				punit_(nullptr),
				group_count_(0),
				group_mask_(0),
				size_(0),
				gate_(0){}


			unordered_flat_table(unordered_flat_table&& u)noexcept
				requires std::is_nothrow_move_constructible_v<HS>:
				h_(move(u.h_)),
				pgroup_(u.pgroup_),
				punit_(u.punit_),
				group_count_(u.group_count_),
				group_mask_(u.group_mask_),
				size_(u.size_),
				gate_(u.gate_) {
				u.to_void();
			}


			unordered_flat_table& operator=(unordered_flat_table&& u)noexcept 
				requires std::is_nothrow_move_assignable_v<HS> {
				if (this == &u)return *this;
				destroy_units_and_free();

				h_ = move(u.h_);
				pgroup_ = u.pgroup_;
				punit_ = u.punit_;
				group_count_ = u.group_count_;
				group_mask_ = u.group_mask_;
				size_ = u.size_;
				gate_ = u.gate_;

				u.to_void();

				return *this;
			}

			~unordered_flat_table()noexcept {
				destroy_units_and_free();
			}

			ret flush()noexcept {
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}

			ret reserve(const std::size_t size)noexcept {
				constexpr std::size_t GT = (~static_cast<std::size_t>(0)) >> 1;

				if (size != 0) {
					std::size_t need = max(group_count_, static_cast<std::size_t>(1));
					while (get_init_gate(need) < size) {
						if (need > GT)XER_UNLIKELY return ret::alloc_failure;
						need <<= 1;
					}
					if (need == group_count_)return ret::success;
					Group* const new_pgroup = try_get_enough_memory(need);
					if (new_pgroup == nullptr)XER_UNLIKELY return ret::alloc_failure;
					change_capacity(new_pgroup, need);
				}
				return ret::success;
			}


			ret insert(const Key& k)noexcept
				requires (!has_value) && std::is_nothrow_copy_constructible_v<Key>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				//size_<gate_<group_count*15
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, k);
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}

			ret insert(Key&& k)noexcept
				requires (!has_value) && std::is_nothrow_move_constructible_v<Key>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, ::xer::forward<Key>(k));
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}


			ret insert(const Key& k,const Value&v)noexcept
				requires (has_value) && 
				std::is_nothrow_copy_constructible_v<Key>&&
				std::is_nothrow_copy_constructible_v<Value>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, k, v);
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}

			ret insert(const Key& k,Value&&v)noexcept
				requires (has_value) && 
				std::is_nothrow_copy_constructible_v<Key>&&
				std::is_nothrow_move_constructible_v<Value>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, k, ::xer::forward<Value>(v));
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}

			ret insert(Key&& k,const Value&v)noexcept
				requires (has_value) && 
				std::is_nothrow_move_constructible_v<Key>&&
				std::is_nothrow_copy_constructible_v<Value>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, ::xer::forward<Key>(k), v);
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}

			ret insert(Key&& k,Value&&v)noexcept
				requires (has_value) && 
				std::is_nothrow_move_constructible_v<Key>&&
				std::is_nothrow_move_constructible_v<Value>{
				if (size_ >= gate_) {
					if (group_count_ != 0)return ret::out_of_space;
					else if (!capacity_init())XER_UNLIKELY return ret::alloc_failure;
				}
				Unit* const pu = try_insert_unckecked(k);
				if (pu == nullptr)return ret::value_collison;

				size_++;
				construct_at(pu, ::xer::forward<Key>(k), ::xer::forward<Value>(v));
				return size_ >= gate_ ? flush_unchecked() : ret::success;
			}
		

			ret erase(const Key& k)noexcept {
				const std::size_t hash_r = h_(k);
				std::size_t index = (hash_r >> 8)&group_mask_;
				const std::uint8_t label = fine_label(hash_r);


				std::size_t step = 0;
				while (step < group_count_) {
					Group* const pg = pgroup_ + index;
#if XER_SSE2_AVAILABLE
					for (std::uint32_t judge = M128i{ pg }.match(label);
						judge != 0;
						judge &= judge - 1) {
						const std::uint32_t i = countr_zero(judge);

						Unit* const pu = punit_ + gtou_index(index, i);

						if (k == get_unit_key(*pu)) {
							pg->ctrls[i] = 0;
							destroy_at(pu);

							size_--;
							if (pg->ofw & (1 << (label & 0x7)))gate_--;
							return ret::success;
						}
					}
#else

					std::uint8_t* const pc = pg->ctrls;
					for (std::uint32_t i = 0; i < 15; i++) {
						if (pc[i] == label) {
							Unit* const pu = punit_ + gtou_index(index, i);

							if (k == get_unit_key(*pu)) {
								pc[i] = 0;
								destroy_at(pu);

								size_--;
								if (pg->ofw & (1 << (label & 0x7)))gate_--;
								return ret::success;
							}
						}

					}
#endif
					if ((pg->ofw & (1 << (label & 0x7))) == 0)break;

					step++;
					index = (index + step) & group_mask_;
				}

				return ret::value_no_found;
			}


			template<typename F>
			requires std::is_nothrow_invocable_r_v<bool,F,const Key&>
			void erase_if_k(F&& f)noexcept {
				erase_if_impl<0>(::xer::forward<F>(f));
			}

			template<typename F>
			requires 
			(has_value)&&
			std::is_nothrow_invocable_r_v<bool, F, const Value&>
			void erase_if_v(F&& f)noexcept {
				erase_if_impl<1>(::xer::forward<F>(f));
			}

			template<typename F>
			requires
			(has_value) &&
			std::is_nothrow_invocable_r_v<bool, F,const Key&, const Value&>
			void erase_if_kv(F&& f)noexcept {
				erase_if_impl<2>(::xer::forward<F>(f));
			}

			FindPtr_t find(const Key& k)noexcept {
				return find_impl(k);
			}

			FindPtrConst_t find(const Key& k)const noexcept {
				return find_impl(k);
			}



			template<typename F>
			requires std::is_nothrow_invocable_r_v<void, F,const Key&>
			void for_each_k(F&& f)const noexcept {
				for_each_impl<true, 0>(::xer::forward<F>(f));
			}

			template<typename F>
			requires 
			(has_value)&&
			std::is_nothrow_invocable_r_v<void,F,Value&>
			void for_each_v(F&& f)noexcept {
				for_each_impl<false, 1>(::xer::forward<F>(f));
			}

			template<typename F>
			requires
			(has_value) &&
			std::is_nothrow_invocable_r_v<void, F,const Value&>
			void for_each_v(F&& f)const noexcept {
				for_each_impl<true, 1>(::xer::forward<F>(f));
			}

			template<typename F>
			requires
			(has_value) &&
			std::is_nothrow_invocable_r_v<void, F,const Key&,Value&>
			void for_each_kv(F&& f)noexcept {
				for_each_impl<false, 2>(::xer::forward<F>(f));
			}

			template<typename F>
			requires
			(has_value) &&
			std::is_nothrow_invocable_r_v<void, F, const Key&,const Value&>
			void for_each_kv(F&& f)const noexcept {
				for_each_impl<true, 2>(::xer::forward<F>(f));
			}
		};
	}



	template<typename Key,typename HS>
	using unordered_flat_set = details_::unordered_flat_table<Key, char, false, HS>;

	template<typename Key,typename Value,typename HS>
	using unordered_flat_map = details_::unordered_flat_table<Key, Value, true, HS>;
}


#endif