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

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "qemu/qemu-print.h"
#include "tcg.h"

/* Include the symbolic backend, using void* as expression type. */

#define SymExpr void*
#include "RuntimeCommon.h"

/* Returning NULL for unimplemented functions is equivalent to concretizing and
 * allows us to run without all symbolic handlers fully implemented. */

#define NOT_IMPLEMENTED NULL

/* A slightly questionable macro to help with the repetitive parts of
 * implementing the symbolic handlers: assuming the existence of concrete
 * arguments "arg1" and "arg2" along with variables "arg1_expr" and "arg2_expr"
 * for the corresponding expressions, it expands into code that returns early if
 * both expressions are NULL and otherwise creates the missing expression.*/

#define BINARY_HELPER_ENSURE_EXPRESSIONS                                       \
    if (arg1_expr == NULL && arg2_expr == NULL) {                              \
        return NULL;                                                           \
    }                                                                          \
                                                                               \
    if (arg1_expr == NULL) {                                                   \
        arg1_expr = _sym_build_integer(arg1, _sym_bits_helper(arg2_expr));     \
    }                                                                          \
                                                                               \
    if (arg2_expr == NULL) {                                                   \
        arg2_expr = _sym_build_integer(arg2, _sym_bits_helper(arg1_expr));     \
    }                                                                          \
                                                                               \
    assert(_sym_bits_helper(arg1_expr) == 32 ||                                \
           _sym_bits_helper(arg1_expr) == 64);                                 \
    assert(_sym_bits_helper(arg2_expr) == 32 ||                                \
           _sym_bits_helper(arg2_expr) == 64);                                 \
    assert(_sym_bits_helper(arg1_expr) == _sym_bits_helper(arg2_expr));

/* This macro declares a binary helper function with 64-bit arguments and
 * defines a 32-bit helper function that delegates to it. Use it instead of the
 * function prototype in helper definitions. */

#define DECL_HELPER_BINARY(name)                                               \
    void *HELPER(sym_##name##_i32)(uint32_t arg1, void *arg1_expr,             \
                                   uint32_t arg2, void *arg2_expr) {           \
        return HELPER(sym_##name##_i64)(arg1, arg1_expr, arg2, arg2_expr);     \
    }                                                                          \
                                                                               \
    void *HELPER(sym_##name##_i64)(uint64_t arg1, void *arg1_expr,             \
                                   uint64_t arg2, void *arg2_expr)

/* To save implementation effort, the macro below defines handlers following the
 * standard scheme of binary operations:
 *
 * 1. Return NULL if both operands are concrete.
 * 2. Create any missing expression.
 * 3. Create an expression representing the operation.
 *
 * For example, DEF_HELPER_BINARY(divu, unsigned_div) defines helpers
 * "helper_sym_divu_i32/i64" backed by the run-time function
 * "_sym_build_unsigned_div". The 32-bit helper just extends the arguments and
 * calls the 64-bit helper. */

#define DEF_HELPER_BINARY(qemu_name, symcc_name)                               \
    DECL_HELPER_BINARY(qemu_name) {                                            \
        BINARY_HELPER_ENSURE_EXPRESSIONS;                                      \
        return _sym_build_##symcc_name(arg1_expr, arg2_expr);                  \
    }


/* Architecture-independent way to get the program counter */
static target_ulong get_pc(CPUArchState *env)
{
    target_ulong pc, cs_base;
    uint32_t flags;

    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);

    return pc;
}


/* The binary helpers */

DEF_HELPER_BINARY(add, add)
DEF_HELPER_BINARY(sub, sub)
DEF_HELPER_BINARY(mul, mul)
DEF_HELPER_BINARY(div, signed_div)
DEF_HELPER_BINARY(divu, unsigned_div)
DEF_HELPER_BINARY(rem, signed_rem)
DEF_HELPER_BINARY(remu, unsigned_rem)
DEF_HELPER_BINARY(and, and)
DEF_HELPER_BINARY(or, or)
DEF_HELPER_BINARY(xor, xor)
DEF_HELPER_BINARY(shift_right, logical_shift_right)
DEF_HELPER_BINARY(arithmetic_shift_right, arithmetic_shift_right)
DEF_HELPER_BINARY(shift_left, shift_left)

void *HELPER(sym_neg)(void *expr)
{
    if (expr == NULL)
        return NULL;

    return _sym_build_neg(expr);
}

DECL_HELPER_BINARY(andc)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;
    return _sym_build_and(arg1_expr, _sym_build_not(arg2_expr));
}

DECL_HELPER_BINARY(eqv)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;
    return _sym_build_not(_sym_build_xor(arg1_expr, arg2_expr));
}

DECL_HELPER_BINARY(nand)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;
    return _sym_build_not(_sym_build_and(arg1_expr, arg2_expr));
}

DECL_HELPER_BINARY(nor)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;
    return _sym_build_not(_sym_build_or(arg1_expr, arg2_expr));
}

DECL_HELPER_BINARY(orc)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;
    return _sym_build_or(arg1_expr, _sym_build_not(arg2_expr));
}

void *HELPER(sym_not)(void *expr)
{
    if (expr == NULL)
        return NULL;

    return _sym_build_not(expr);
}

void *HELPER(sym_muluh_i64)(uint64_t arg1, void *arg1_expr,
                            uint64_t arg2, void *arg2_expr)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    assert(_sym_bits_helper(arg1_expr) == 64 &&
           _sym_bits_helper(arg2_expr) == 64);
    void *full_result = _sym_build_mul(_sym_build_zext(arg1_expr, 64),
                                       _sym_build_zext(arg2_expr, 64));
    return _sym_extract_helper(full_result, 127, 64);
}

void *HELPER(sym_sext)(void *expr, uint64_t target_length)
{
    if (expr == NULL)
        return NULL;

    size_t current_bits = _sym_bits_helper(expr);
    size_t bits_to_keep = target_length * 8;
    void *shift_distance_expr = _sym_build_integer(
        current_bits - bits_to_keep, current_bits);

    return _sym_build_arithmetic_shift_right(
        _sym_build_shift_left(expr, shift_distance_expr),
        shift_distance_expr);
}

void *HELPER(sym_zext)(void *expr, uint64_t target_length)
{
    if (expr == NULL)
        return NULL;

    size_t current_bits = _sym_bits_helper(expr);
    size_t desired_bits = target_length * 8;

    return _sym_build_and(
        expr,
        _sym_build_integer((1ull << desired_bits) - 1, current_bits));
}

void *HELPER(sym_sext_i32_i64)(void *expr)
{
    if (expr == NULL)
        return NULL;

    assert(_sym_bits_helper(expr) == 32);
    return _sym_build_sext(expr, 32); /* extend by 32 */
}

void *HELPER(sym_zext_i32_i64)(void *expr)
{
    if (expr == NULL)
        return NULL;

    assert(_sym_bits_helper(expr) == 32);
    return _sym_build_zext(expr, 32); /* extend by 32 */
}

void *HELPER(sym_trunc_i64_i32)(void *expr)
{
    if (expr == NULL)
        return NULL;

    assert(_sym_bits_helper(expr) == 64);
    return _sym_build_trunc(expr, 32);
}

void *HELPER(sym_bswap)(void *expr, uint64_t length)
{
    if (expr == NULL)
        return NULL;

    /* The implementation follows the alternative implementations of
     * tcg_gen_bswap* in tcg-op.c (which handle architectures that don't support
     * bswap directly). */

    size_t bits = _sym_bits_helper(expr);
    void *eight = _sym_build_integer(8, bits);
    void *sixteen = _sym_build_integer(16, bits);
    void *thirty_two = _sym_build_integer(32, bits);
    void *forty_eight = _sym_build_integer(48, bits);

    switch (length) {
    case 2:
        return _sym_build_or(
            _sym_build_shift_left(HELPER(sym_zext)(expr, 1), eight),
            _sym_build_logical_shift_right(expr, eight));
    case 4: {
        void *mask = _sym_build_integer(0x00ff00ff, bits);

        /* This is equivalent to the temporary "ret" after the first block. */
        void *first_block = _sym_build_or(
            _sym_build_and(
                _sym_build_logical_shift_right(expr, eight),
                mask),
            _sym_build_shift_left(_sym_build_and(expr, mask), eight));

        /* This is the second block. */
        if (bits == 32)
            return _sym_build_or(
                _sym_build_logical_shift_right(first_block, sixteen),
                _sym_build_shift_left(first_block, sixteen));
        else
            return _sym_build_or(
                _sym_build_logical_shift_right(first_block, sixteen),
                _sym_build_logical_shift_right(
                    _sym_build_shift_left(first_block, forty_eight),
                    thirty_two));
    }
    case 8: {
        void *mask1 = _sym_build_integer(0x00ff00ff00ff00ffull, 64);
        void *mask2 = _sym_build_integer(0x0000ffff0000ffffull, 64);

        /* This is equivalent to the temporary "ret" after the first block. */
        void *first_block = _sym_build_or(
            _sym_build_and(_sym_build_logical_shift_right(expr, eight), mask1),
            _sym_build_shift_left(_sym_build_and(expr, mask1), eight));

        /* Here we replicate the second block. */
        void *second_block = _sym_build_or(
            _sym_build_and(
                _sym_build_logical_shift_right(first_block, sixteen),
                mask2),
            _sym_build_shift_left(_sym_build_and(first_block, mask2), sixteen));

        /* And finally the third block. */
        return _sym_build_or(
            _sym_build_logical_shift_right(second_block, thirty_two),
            _sym_build_shift_left(second_block, thirty_two));
    }
    default:
        g_assert_not_reached();
    }
}

static void *sym_load_guest_internal(CPUArchState *env,
                                     target_ulong addr, void *addr_expr,
                                     uint64_t load_length, uint8_t result_length,
                                     target_ulong mmu_idx)
{
    /* Try an alternative address */
    if (addr_expr != NULL)
        _sym_push_path_constraint(
            _sym_build_equal(
                addr_expr, _sym_build_integer(addr, sizeof(addr) * 8)),
            true, get_pc(env));

    void *host_addr = tlb_vaddr_to_host(env, addr, MMU_DATA_LOAD, mmu_idx);
    void *memory_expr = _sym_read_memory((uint8_t*)host_addr, load_length, true);

    if (load_length == result_length || memory_expr == NULL)
        return memory_expr;
    else
        return _sym_build_zext(memory_expr, (result_length - load_length) * 8);
}

void *HELPER(sym_load_guest_i32)(CPUArchState *env,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length, target_ulong mmu_idx)
{
    return sym_load_guest_internal(env, addr, addr_expr, length, 4, mmu_idx);
}

void *HELPER(sym_load_guest_i64)(CPUArchState *env,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length, target_ulong mmu_idx)
{
    return sym_load_guest_internal(env, addr, addr_expr, length, 8, mmu_idx);
}

static void sym_store_guest_internal(CPUArchState *env,
                                     uint64_t value, void *value_expr,
                                     target_ulong addr, void *addr_expr,
                                     uint64_t length, target_ulong mmu_idx)
{
    /* Try an alternative address */
    if (addr_expr != NULL)
        _sym_push_path_constraint(
            _sym_build_equal(
                addr_expr, _sym_build_integer(addr, sizeof(addr) * 8)),
            true, get_pc(env));

    void *host_addr = tlb_vaddr_to_host(env, addr, MMU_DATA_STORE, mmu_idx);
    _sym_write_memory((uint8_t*)host_addr, length, value_expr, true);
}

void HELPER(sym_store_guest_i32)(CPUArchState *env,
                                 uint32_t value, void *value_expr,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length, target_ulong mmu_idx)
{
    return sym_store_guest_internal(
        env, value, value_expr, addr, addr_expr, length, mmu_idx);
}

void HELPER(sym_store_guest_i64)(CPUArchState *env,
                                 uint64_t value, void *value_expr,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length, target_ulong mmu_idx)
{
    return sym_store_guest_internal(
        env, value, value_expr, addr, addr_expr, length, mmu_idx);
}

static void *sym_load_host_internal(void *addr, uint64_t offset,
                                    uint64_t load_length, uint64_t result_length)
{
    void *memory_expr = _sym_read_memory(
        (uint8_t*)addr + offset, load_length, true);

    if (load_length == result_length || memory_expr == NULL)
        return memory_expr;
    else
        return _sym_build_zext(memory_expr, (result_length - load_length) * 8);
}

void *HELPER(sym_load_host_i32)(void *addr, uint64_t offset, uint64_t length)
{
    return sym_load_host_internal(addr, offset, length, 4);
}

void *HELPER(sym_load_host_i64)(void *addr, uint64_t offset, uint64_t length)
{
    return sym_load_host_internal(addr, offset, length, 8);
}

void HELPER(sym_store_host_i32)(uint32_t value, void *value_expr,
                                void *addr,
                                uint64_t offset, uint64_t length)
{
    _sym_write_memory((uint8_t*)addr + offset, length, value_expr, true);
}

void HELPER(sym_store_host_i64)(uint64_t value, void *value_expr,
                                void *addr,
                                uint64_t offset, uint64_t length)
{
    _sym_write_memory((uint8_t*)addr + offset, length, value_expr, true);
}

DECL_HELPER_BINARY(rotate_left)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    /* The implementation follows the alternative implementation of
     * tcg_gen_rotl_i64 in tcg-op.c (which handles architectures that don't
     * support rotl directly). */

    uint8_t bits = _sym_bits_helper(arg1_expr);
    return _sym_build_or(
        _sym_build_shift_left(arg1_expr, arg2_expr),
        _sym_build_logical_shift_right(
            arg1_expr,
            _sym_build_sub(_sym_build_integer(bits, bits), arg2_expr)));
}

DECL_HELPER_BINARY(rotate_right)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    /* The implementation follows the alternative implementation of
     * tcg_gen_rotr_i64 in tcg-op.c (which handles architectures that don't
     * support rotr directly). */

    uint8_t bits = _sym_bits_helper(arg1_expr);
    return _sym_build_or(
        _sym_build_logical_shift_right(arg1_expr, arg2_expr),
        _sym_build_shift_left(
            arg1_expr,
            _sym_build_sub(_sym_build_integer(bits, bits), arg2_expr)));
}

void *HELPER(sym_extract_i32)(void *expr, uint32_t ofs, uint32_t len)
{
    return HELPER(sym_extract_i64)(expr, ofs, len);
}

void *HELPER(sym_extract_i64)(void *expr, uint64_t ofs, uint64_t len)
{
    if (expr == NULL)
        return NULL;

    return _sym_build_zext(
        _sym_extract_helper(expr, ofs + len - 1, ofs),
        _sym_bits_helper(expr) - len);
}

void *HELPER(sym_extract2_i32)(uint32_t ah, void *ah_expr,
                               uint32_t al, void *al_expr,
                               uint64_t ofs)
{
    if (ah_expr == NULL && al_expr == NULL)
        return NULL;

    if (ah_expr == NULL)
        ah_expr = _sym_build_integer(ah, 32);

    if (al_expr == NULL)
        al_expr = _sym_build_integer(al, 32);

    /* The implementation follows the alternative implementation of
     * tcg_gen_extract2_i32 in tcg-op.c (which handles architectures that don't
     * support extract2 directly). */

    if (ofs == 0)
        return al_expr;
    if (ofs == 32)
        return ah_expr;

    return HELPER(sym_deposit_i32)(
        al >> ofs,
        _sym_build_logical_shift_right(
            al_expr,
            _sym_build_integer(ofs, 32)),
        ah, ah_expr,
        32 - ofs,
        ofs);
}

void *HELPER(sym_extract2_i64)(uint64_t ah, void *ah_expr,
                               uint64_t al, void *al_expr,
                               uint64_t ofs)
{
    if (ah_expr == NULL && al_expr == NULL)
        return NULL;

    if (ah_expr == NULL)
        ah_expr = _sym_build_integer(ah, 64);

    if (al_expr == NULL)
        al_expr = _sym_build_integer(al, 64);

    /* The implementation follows the alternative implementation of
     * tcg_gen_extract2_i64 in tcg-op.c (which handles architectures that don't
     * support extract2 directly). */

    if (ofs == 0)
        return al_expr;
    if (ofs == 64)
        return ah_expr;

    return HELPER(sym_deposit_i64)(
        al >> ofs,
        _sym_build_logical_shift_right(
            al_expr,
            _sym_build_integer(ofs, 64)),
        ah, ah_expr,
        64 - ofs,
        ofs);
}

void *HELPER(sym_sextract_i32)(void *expr, uint32_t ofs, uint32_t len)
{
    return HELPER(sym_sextract_i64)(expr, ofs, len);
}

void *HELPER(sym_sextract_i64)(void *expr, uint64_t ofs, uint64_t len)
{
    if (expr == NULL)
        return NULL;

    return _sym_build_sext(
        _sym_extract_helper(expr, ofs + len - 1, ofs),
        _sym_bits_helper(expr) - len);
}

void *HELPER(sym_deposit_i32)(uint32_t arg1, void *arg1_expr,
                              uint32_t arg2, void *arg2_expr,
                              uint32_t ofs, uint32_t len)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    /* The symbolic implementation follows the alternative concrete
     * implementation of tcg_gen_deposit_i32 in tcg-op.c (which handles
     * architectures that don't support deposit directly). */

    uint32_t mask = (1u << len) - 1;
    return _sym_build_or(
        _sym_build_and(
            arg1_expr,
            _sym_build_integer(~(mask << ofs), 32)),
        _sym_build_shift_left(
            _sym_build_and(arg2_expr, _sym_build_integer(mask, 32)),
            _sym_build_integer(ofs, 32)));
}

void *HELPER(sym_deposit_i64)(uint64_t arg1, void *arg1_expr,
                              uint64_t arg2, void *arg2_expr,
                              uint64_t ofs, uint64_t len)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    /* The symbolic implementation follows the alternative concrete
     * implementation of tcg_gen_deposit_i64 in tcg-op.c (which handles
     * architectures that don't support deposit directly). */

    uint64_t mask = (1ull << len) - 1;
    return _sym_build_or(
        _sym_build_and(
            arg1_expr,
            _sym_build_integer(~(mask << ofs), 64)),
        _sym_build_shift_left(
            _sym_build_and(arg2_expr, _sym_build_integer(mask, 64)),
            _sym_build_integer(ofs, 64)));
}

static void *sym_setcond_internal(CPUArchState *env,
                                  uint64_t arg1, void *arg1_expr,
                                  uint64_t arg2, void *arg2_expr,
                                  int32_t cond, uint64_t result,
                                  uint8_t result_bits)
{
    BINARY_HELPER_ENSURE_EXPRESSIONS;

    void *(*handler)(void *, void*);
    switch (cond) {
    case TCG_COND_EQ:
        handler = _sym_build_equal;
        break;
    case TCG_COND_NE:
        handler = _sym_build_not_equal;
        break;
    case TCG_COND_LT:
        handler = _sym_build_signed_less_than;
        break;
    case TCG_COND_GE:
        handler = _sym_build_signed_greater_equal;
        break;
    case TCG_COND_LE:
        handler = _sym_build_signed_less_equal;
        break;
    case TCG_COND_GT:
        handler = _sym_build_signed_greater_than;
        break;
    case TCG_COND_LTU:
        handler = _sym_build_unsigned_less_than;
        break;
    case TCG_COND_GEU:
        handler = _sym_build_unsigned_greater_equal;
        break;
    case TCG_COND_LEU:
        handler = _sym_build_unsigned_less_equal;
        break;
    case TCG_COND_GTU:
        handler = _sym_build_unsigned_greater_than;
        break;
    default:
        g_assert_not_reached();
    }

    void *condition = handler(arg1_expr, arg2_expr);
    _sym_push_path_constraint(condition, result, get_pc(env));

    assert(result_bits > 1);
    return _sym_build_zext(_sym_build_bool_to_bit(condition),
                           result_bits - 1);
}

void *HELPER(sym_setcond_i32)(CPUArchState *env,
                              uint32_t arg1, void *arg1_expr,
                              uint32_t arg2, void *arg2_expr,
                              int32_t cond, uint32_t result)
{
    return sym_setcond_internal(
        env, arg1, arg1_expr, arg2, arg2_expr, cond, result, 32);
}

void *HELPER(sym_setcond_i64)(CPUArchState *env,
                              uint64_t arg1, void *arg1_expr,
                              uint64_t arg2, void *arg2_expr,
                              int32_t cond, uint64_t result)
{
    return sym_setcond_internal(
        env, arg1, arg1_expr, arg2, arg2_expr, cond, result, 64);
}

void HELPER(sym_notify_call)(uint64_t return_address)
{
    _sym_notify_call(return_address);
}

void HELPER(sym_notify_return)(uint64_t return_address)
{
    _sym_notify_ret(return_address);
}

void HELPER(sym_notify_block)(uint64_t block_id)
{
    _sym_notify_basic_block(block_id);
}

void HELPER(sym_collect_garbage)(void)
{
    _sym_collect_garbage();
}
