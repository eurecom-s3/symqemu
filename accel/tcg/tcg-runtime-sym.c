#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/qemu-print.h"

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
    }

#define BINARY_HELPER_BODY(symcc_name)                                         \
    BINARY_HELPER_ENSURE_EXPRESSIONS                                           \
    return _sym_build_##symcc_name(arg1_expr, arg2_expr);

/* To save implementation effort, the macro below defines handlers following the
 * standard scheme of binary operations:
 *
 * 1. Return NULL if both operands are concrete.
 * 2. Create any missing expression.
 * 3. Create an expression representing the operation.
 *
 * For example, DEF_HELPER_BINARY(divu, unsigned_div) defines helpers
 * "helper_sym_divu_i32/i64" backed by the run-time function
 * "_sym_build_unsigned_div". */

/* TODO Probably we only need a single helper per operation, independent of the
 * argument width. This would, however, complicate the instrumentation because
 * we would need to extend the concrete values in the 32-bit case before calling
 * the helper. */

#define DEF_HELPER_BINARY(qemu_name, symcc_name)                               \
    void *HELPER(sym_##qemu_name##_i64)(uint64_t arg1, void *arg1_expr,        \
                                        uint64_t arg2, void *arg2_expr) {      \
        BINARY_HELPER_BODY(symcc_name)                                         \
    }                                                                          \
                                                                               \
    void *HELPER(sym_##qemu_name##_i32)(uint32_t arg1, void *arg1_expr,        \
                                        uint32_t arg2, void *arg2_expr) {      \
        BINARY_HELPER_BODY(symcc_name)                                         \
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

void *HELPER(sym_neg)(void *expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_andc_i32)(uint32_t arg1, void *arg1_expr,
                           uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_andc_i64)(uint64_t arg1, void *arg1_expr,
                           uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_eqv_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_eqv_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_nand_i32)(uint32_t arg1, void *arg1_expr,
                           uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_nand_i64)(uint64_t arg1, void *arg1_expr,
                           uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_nor_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_nor_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_orc_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_orc_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_not)(void *expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_sext_or_trunc)(void *expr, uint64_t target_length)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_zext_or_trunc)(void *expr, uint64_t target_length)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_bswap)(void *expr, uint64_t length)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_load_guest)(target_ulong addr, void *addr_expr, uint64_t length)
{
    /* TODO try an alternative address; cast the address to uint64_t */
    return _sym_read_memory((uint8_t*)addr, length, true);
}

void HELPER(sym_store_guest_i32)(uint32_t value, void *value_expr,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length)
{
    /* TODO try alternative address */

    if (value_expr == NULL)
        value_expr = _sym_build_integer(value, 32);

    _sym_write_memory((uint8_t*)addr, length, value_expr, true);
}

void HELPER(sym_store_guest_i64)(uint64_t value, void *value_expr,
                                 target_ulong addr, void *addr_expr,
                                 uint64_t length)
{
    /* TODO try alternative address */

    if (value_expr == NULL)
        value_expr = _sym_build_integer(value, 64);

    _sym_write_memory((uint8_t*)addr, length, value_expr, true);
}

void *HELPER(sym_load_host)(void *addr, uint64_t offset, uint64_t length)
{
    return _sym_read_memory((uint8_t*)addr + offset, length, true);
}

void HELPER(sym_store_host_i32)(uint32_t value, void *value_expr,
                                void *addr,
                                uint64_t offset, uint64_t length)
{
    if (value_expr == NULL)
        value_expr = _sym_build_integer(value, 32);

    _sym_write_memory((uint8_t*)addr + offset, length, value_expr, true);
}

void HELPER(sym_store_host_i64)(uint64_t value, void *value_expr,
                                void *addr,
                                uint64_t offset, uint64_t length)
{
    if (value_expr == NULL)
        value_expr = _sym_build_integer(value, 64);

    _sym_write_memory((uint8_t*)addr + offset, length, value_expr, true);
}

void *HELPER(sym_shift_right_i32)(uint32_t arg1, void *arg1_expr,
                                  uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_shift_right_i64)(uint64_t arg1, void *arg1_expr,
                                  uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_arithmetic_shift_right_i32)(uint32_t arg1, void *arg1_expr,
                                             uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_arithmetic_shift_right_i64)(uint64_t arg1, void *arg1_expr,
                                             uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_shift_left_i32)(uint32_t arg1, void *arg1_expr,
                                 uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_shift_left_i64)(uint64_t arg1, void *arg1_expr,
                                 uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_rotate_left_i32)(uint32_t arg1, void *arg1_expr,
                                  uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_rotate_left_i64)(uint64_t arg1, void *arg1_expr,
                                  uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_rotate_right_i32)(uint32_t arg1, void *arg1_expr,
                                   uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_rotate_right_i64)(uint64_t arg1, void *arg1_expr,
                                   uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_extract_i32)(uint32_t arg, void *arg_expr,
                              uint32_t ofs, uint32_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_extract_i64)(uint64_t arg, void *arg_expr,
                              uint64_t ofs, uint64_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_extract2_i32)(uint32_t ah, void *ah_expr,
                               uint32_t al, void *al_expr,
                               uint64_t ofs)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_extract2_i64)(uint64_t ah, void *ah_expr,
                               uint64_t al, void *al_expr,
                               uint64_t ofs)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_sextract_i32)(uint32_t arg, void *arg_expr,
                               uint32_t ofs, uint32_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_sextract_i64)(uint64_t arg, void *arg_expr,
                               uint64_t ofs, uint64_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_deposit_i32)(uint32_t arg1, void *arg1_expr,
                              uint32_t arg2, void *arg2_expr,
                              uint32_t ofs, uint32_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_deposit_i64)(uint64_t arg1, void *arg1_expr,
                              uint64_t arg2, void *arg2_expr,
                              uint64_t ofs, uint64_t len)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_setcond_i32)(uint32_t arg1, void *arg1_expr,
                              uint32_t arg2, void *arg2_expr,
                              int32_t cond, uint32_t result)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_setcond_i64)(uint64_t arg1, void *arg1_expr,
                              uint64_t arg2, void *arg2_expr,
                              int32_t cond, uint64_t result)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}
