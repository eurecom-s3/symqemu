#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/cpu_ldst.h"
#include "qemu/qemu-print.h"
#include "tcg/tcg.h"
#include "exec/translation-block.h"

#include "accel/tcg/tcg-runtime-sym-common.h"

/* Include the symbolic backend, using void* as expression type. */

#define SymExpr void*
#include "RuntimeCommon.h"

void *build_and_push_path_constraint(CPUArchState *env, void *arg1_expr, void *arg2_expr, uint32_t comparison_operator, uint8_t is_taken){
    void *(*handler)(void *, void*);
    switch (comparison_operator) {
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

    void *condition_symbol = handler(arg1_expr, arg2_expr);
    _sym_push_path_constraint(condition_symbol, is_taken, get_pc(env));

    return condition_symbol;
}

/* Architecture-independent way to get the program counter */
target_ulong get_pc(CPUArchState *env)
{
    vaddr pc;
    uint64_t cs_base;
    uint32_t flags;

    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);

    return pc;
}

void *sym_rotate_left(void *arg1_expr, void *arg2_expr) {
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

void *sym_rotate_right(void *arg1_expr, void *arg2_expr) {

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
