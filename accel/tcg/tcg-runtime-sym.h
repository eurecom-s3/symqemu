/*
 * This file is part of SymQEMU.
 *
 * SymQEMU is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * SymQEMU is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SymQEMU. If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Symbolic helpers
 *
 * The helpers in this file perform symbolic computations corresponding to TCG's
 * instruction set. The TCG code-generation functions in tcg-op.h emit calls to
 * the helpers where necessary, and the helpers call into the symbolic run-time
 * library to create expressions or trigger the solver.
 *
 * For now, we assume a 64-bit host, running a 32-bit or 64-bit guest
 * architecture. This saves us from having to implement symbolic handling for
 * the special cases that arise when a 32-bit machine needs to deal with 64-bit
 * values; we don't expect anyone to run on a 32-bit host anyway.
 */

/* A macro to save some typing: SYM_HELPER_BINARY(foo) creates two helpers
 * "sym_foo_i32" and "sym_foo_i64", each taking two concrete arguments along
 * with the corresponding symbolic expressions, and returning a symbolic
 * expression. This covers a lot of instructions (e.g., add, sub, mul, div). */
#define SYM_HELPER_BINARY(name)                                         \
  DEF_HELPER_FLAGS_4(sym_##name##_i32, TCG_CALL_NO_RWG_SE, ptr,                \
                     i32, ptr, i32, ptr)                                       \
  DEF_HELPER_FLAGS_4(sym_##name##_i64, TCG_CALL_NO_RWG_SE, ptr,                \
                     i64, ptr, i64, ptr)

/* Arithmetic */
SYM_HELPER_BINARY(add)
SYM_HELPER_BINARY(sub)
SYM_HELPER_BINARY(mul)
SYM_HELPER_BINARY(div)
SYM_HELPER_BINARY(divu)
SYM_HELPER_BINARY(rem)
SYM_HELPER_BINARY(remu)
DEF_HELPER_FLAGS_1(sym_neg, TCG_CALL_NO_RWG_SE, ptr, ptr)

/* Shifts */
SYM_HELPER_BINARY(shift_right)
SYM_HELPER_BINARY(arithmetic_shift_right)
SYM_HELPER_BINARY(shift_left)
SYM_HELPER_BINARY(rotate_left)
SYM_HELPER_BINARY(rotate_right)

/* Logical operations */
SYM_HELPER_BINARY(and)
SYM_HELPER_BINARY(or)
SYM_HELPER_BINARY(xor)
SYM_HELPER_BINARY(andc)
SYM_HELPER_BINARY(eqv)
SYM_HELPER_BINARY(nand)
SYM_HELPER_BINARY(nor)
SYM_HELPER_BINARY(orc)
DEF_HELPER_FLAGS_1(sym_not, TCG_CALL_NO_RWG_SE, ptr, ptr)

#undef SYM_HELPER_BINARY

/* Multi-word arithmetic: in most cases, we force TCG to fall back to the less
 * efficient implementations of these instructions in terms of single-word
 * arithmetic. We can handle those symbolically, and this way we don't have to
 * work around problems related to the large number of inputs and outputs
 * required by multi-word arithmetic. Multiplication, however, needs a
 * helper. */
DEF_HELPER_FLAGS_4(sym_muluh_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)

/* Extension and truncation */
DEF_HELPER_FLAGS_2(sym_sext, TCG_CALL_NO_RWG_SE, ptr, ptr, i64)
DEF_HELPER_FLAGS_2(sym_zext, TCG_CALL_NO_RWG_SE, ptr, ptr, i64)
DEF_HELPER_FLAGS_1(sym_sext_i32_i64, TCG_CALL_NO_RWG_SE, ptr, ptr)
DEF_HELPER_FLAGS_1(sym_zext_i32_i64, TCG_CALL_NO_RWG_SE, ptr, ptr)
DEF_HELPER_FLAGS_1(sym_trunc_i64_i32, TCG_CALL_NO_RWG_SE, ptr, ptr)

/* Byte swapping */
DEF_HELPER_FLAGS_2(sym_bswap, TCG_CALL_NO_RWG_SE, ptr, ptr, i64)

/* Guest memory */
DEF_HELPER_FLAGS_5(sym_load_guest_i32, TCG_CALL_NO_RWG, ptr, env, dh_alias_tl, ptr, i64, dh_alias_tl)
DEF_HELPER_FLAGS_5(sym_load_guest_i64, TCG_CALL_NO_RWG, ptr, env, dh_alias_tl, ptr, i64, dh_alias_tl)
DEF_HELPER_FLAGS_7(sym_store_guest_i32, TCG_CALL_NO_RWG, void, env, i32, ptr,
                   dh_alias_tl, ptr, i64, dh_alias_tl)
DEF_HELPER_FLAGS_7(sym_store_guest_i64, TCG_CALL_NO_RWG, void, env, i64, ptr,
                   dh_alias_tl, ptr, i64, dh_alias_tl)

/* Host memory */
DEF_HELPER_FLAGS_3(sym_load_host_i32, TCG_CALL_NO_RWG_SE, ptr, ptr, i64, i64)
DEF_HELPER_FLAGS_3(sym_load_host_i64, TCG_CALL_NO_RWG_SE, ptr, ptr, i64, i64)
DEF_HELPER_FLAGS_5(sym_store_host_i32, TCG_CALL_NO_RWG, void, i32, ptr, ptr,
                   i64, i64)
DEF_HELPER_FLAGS_5(sym_store_host_i64, TCG_CALL_NO_RWG, void, i64, ptr, ptr,
                   i64, i64)

/* Bit fields */
DEF_HELPER_FLAGS_3(sym_extract_i32, TCG_CALL_NO_RWG_SE, ptr, ptr, i32, i32)
DEF_HELPER_FLAGS_3(sym_extract_i64, TCG_CALL_NO_RWG_SE, ptr, ptr, i64, i64)
DEF_HELPER_FLAGS_3(sym_sextract_i32, TCG_CALL_NO_RWG_SE, ptr, ptr, i32, i32)
DEF_HELPER_FLAGS_3(sym_sextract_i64, TCG_CALL_NO_RWG_SE, ptr, ptr, i64, i64)
DEF_HELPER_FLAGS_5(sym_extract2_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr, i64)
DEF_HELPER_FLAGS_5(sym_extract2_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr, i64)
DEF_HELPER_FLAGS_6(sym_deposit_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr, i32, i32)
DEF_HELPER_FLAGS_6(sym_deposit_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr, i64, i64)

/* Conditionals */
DEF_HELPER_FLAGS_7(sym_setcond_i32, TCG_CALL_NO_RWG, ptr, env, i32, ptr, i32, ptr, s32, i32)
DEF_HELPER_FLAGS_7(sym_setcond_i64, TCG_CALL_NO_RWG, ptr, env, i64, ptr, i64, ptr, s32, i64)

/* Context tracking */
DEF_HELPER_FLAGS_1(sym_notify_call, TCG_CALL_NO_RWG, void, i64)
DEF_HELPER_FLAGS_1(sym_notify_return, TCG_CALL_NO_RWG, void, i64)
DEF_HELPER_FLAGS_1(sym_notify_block, TCG_CALL_NO_RWG, void, i64)

/* Garbage collection */
DEF_HELPER_FLAGS_0(sym_collect_garbage, TCG_CALL_NO_RWG, void)

/* TODO clz, ctz, clrsb, ctpop; vector operations; helpers for atomic operations (?) */

/* The extrl and extrh instructions aren't emitted on 64-bit hosts. If we ever
 * extend support to other host architectures, we need to implement them. The
 * same applies to brcond2_i32 and setcond2_i32. */
