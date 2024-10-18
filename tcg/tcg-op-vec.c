/*
 * Tiny Code Generator for QEMU
 *
 * Copyright (c) 2018 Linaro, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "tcg/tcg.h"
#include "tcg/tcg-temp-internal.h"
#include "tcg/tcg-op-common.h"
#include "tcg/tcg-mo.h"
#include "tcg-internal.h"

static int vec_size(TCGv_vec vector){
    switch(tcgv_vec_temp(vector)->base_type) {
        case TCG_TYPE_V64:
            return 64;
        case TCG_TYPE_V128:
            return 128;
        case TCG_TYPE_V256:
            return 256;
        default:
            g_assert_not_reached();
    }
}

/* Adds TCG ops for storing a vector TCG variable in a heap buffer.
 *
 * The caller must add TCG ops for freeing the buffer.
 *
 * Returns
 *      A TCG variable that will contain the buffer address at run time.
 */
static TCGv_ptr store_vector_in_memory(TCGv_vec vector){
    TCGv_ptr buffer_address = tcg_temp_new_ptr();
    gen_helper_malloc(buffer_address, tcg_constant_i64(vec_size(vector) / 8));

    /* store vector at buffer_address */
    vec_gen_3(
            INDEX_op_st_vec,
            tcgv_vec_temp(vector)->base_type,
            0,
            tcgv_vec_arg(vector),
            tcgv_ptr_arg(buffer_address),
            0
    );

    return buffer_address;
}

/* Adds instrumentation TCG ops for an instruction of the form vec = vec <op> vec.
 *
 * Args
 *      vece: element size in bits = 8 * 2^vece
 *      r: output operand
 *      a: first input operand
 *      b: second input operand
 *      sym_helper: function for calling the symbolic helper
 */
static void vec_vec_op_instrumentation(
        unsigned vece,
        TCGv_vec r, TCGv_vec a, TCGv_vec b,
        void (*sym_helper)(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_i64, TCGv_i64)
) {
    TCGv_ptr buffer_address_a = store_vector_in_memory(a);
    TCGv_ptr buffer_address_b = store_vector_in_memory(b);
    int size_a = vec_size(a);
    int size_b = vec_size(b);
    int size_r = vec_size(r);

    g_assert(size_a == size_b && size_b == size_r);

    sym_helper(
            tcgv_vec_expr(r),
            buffer_address_a,
            tcgv_vec_expr(a),
            buffer_address_b,
            tcgv_vec_expr(b),
            tcg_constant_i64(size_a),
            tcg_constant_i64(vece)
    );

    gen_helper_free(buffer_address_a);
    gen_helper_free(buffer_address_b);
}

/*
 * Adds instrumentation TCG ops for an instruction of the form vec = vec <op> i32.
 *
 * Args
 *      vece: element size in bits = 8 * 2^vece
 *      r: output operand
 *      a: first input operand
 *      b: second input operand
 *      sym_helper: function for calling the symbolic helper
 */
static void vec_int32_op_instrumentation(
        unsigned vece,
        TCGv_vec r, TCGv_vec a, TCGv_i32 b,
        void (*sym_helper)(TCGv_ptr, TCGv_ptr, TCGv_ptr, TCGv_i32, TCGv_ptr, TCGv_i64, TCGv_i64)
) {
    TCGv_ptr buffer_address_a = store_vector_in_memory(a);
    int size_a = vec_size(a);
    int size_r = vec_size(r);

    g_assert(size_a == size_r);

    sym_helper(
            tcgv_vec_expr(r),
            buffer_address_a,
            tcgv_vec_expr(a),
            b,
            tcgv_i32_expr(b),
            tcg_constant_i64(size_a),
            tcg_constant_i64(vece)
    );

    gen_helper_free(buffer_address_a);
}

/*
 * Vector optional opcode tracking.
 * Except for the basic logical operations (and, or, xor), and
 * data movement (mov, ld, st, dupi), many vector opcodes are
 * optional and may not be supported on the host.  Thank Intel
 * for the irregularity in their instruction set.
 *
 * The gvec expanders allow custom vector operations to be composed,
 * generally via the .fniv callback in the GVecGen* structures.  At
 * the same time, in deciding whether to use this hook we need to
 * know if the host supports the required operations.  This is
 * presented as an array of opcodes, terminated by 0.  Each opcode
 * is assumed to be expanded with the given VECE.
 *
 * For debugging, we want to validate this array.  Therefore, when
 * tcg_ctx->vec_opt_opc is non-NULL, the tcg_gen_*_vec expanders
 * will validate that their opcode is present in the list.
 */
static void tcg_assert_listed_vecop(TCGOpcode op)
{
#ifdef CONFIG_DEBUG_TCG
    const TCGOpcode *p = tcg_ctx->vecop_list;
    if (p) {
        for (; *p; ++p) {
            if (*p == op) {
                return;
            }
        }
        g_assert_not_reached();
    }
#endif
}

bool tcg_can_emit_vecop_list(const TCGOpcode *list,
                             TCGType type, unsigned vece)
{
    if (list == NULL) {
        return true;
    }

    for (; *list; ++list) {
        TCGOpcode opc = *list;

#ifdef CONFIG_DEBUG_TCG
        switch (opc) {
        case INDEX_op_and_vec:
        case INDEX_op_or_vec:
        case INDEX_op_xor_vec:
        case INDEX_op_mov_vec:
        case INDEX_op_dup_vec:
        case INDEX_op_dup2_vec:
        case INDEX_op_ld_vec:
        case INDEX_op_st_vec:
        case INDEX_op_bitsel_vec:
            /* These opcodes are mandatory and should not be listed.  */
            g_assert_not_reached();
        case INDEX_op_not_vec:
            /* These opcodes have generic expansions using the above.  */
            g_assert_not_reached();
        default:
            break;
        }
#endif

        if (tcg_can_emit_vec_op(opc, type, vece)) {
            continue;
        }

        /*
         * The opcode list is created by front ends based on what they
         * actually invoke.  We must mirror the logic in the routines
         * below for generic expansions using other opcodes.
         */
        switch (opc) {
        case INDEX_op_neg_vec:
            if (tcg_can_emit_vec_op(INDEX_op_sub_vec, type, vece)) {
                continue;
            }
            break;
        case INDEX_op_abs_vec:
            if (tcg_can_emit_vec_op(INDEX_op_sub_vec, type, vece)
                && (tcg_can_emit_vec_op(INDEX_op_smax_vec, type, vece) > 0
                    || tcg_can_emit_vec_op(INDEX_op_sari_vec, type, vece) > 0
                    || tcg_can_emit_vec_op(INDEX_op_cmp_vec, type, vece))) {
                continue;
            }
            break;
        case INDEX_op_usadd_vec:
            if (tcg_can_emit_vec_op(INDEX_op_umin_vec, type, vece) ||
                tcg_can_emit_vec_op(INDEX_op_cmp_vec, type, vece)) {
                continue;
            }
            break;
        case INDEX_op_ussub_vec:
            if (tcg_can_emit_vec_op(INDEX_op_umax_vec, type, vece) ||
                tcg_can_emit_vec_op(INDEX_op_cmp_vec, type, vece)) {
                continue;
            }
            break;
        case INDEX_op_cmpsel_vec:
        case INDEX_op_smin_vec:
        case INDEX_op_smax_vec:
        case INDEX_op_umin_vec:
        case INDEX_op_umax_vec:
            if (tcg_can_emit_vec_op(INDEX_op_cmp_vec, type, vece)) {
                continue;
            }
            break;
        default:
            break;
        }
        return false;
    }
    return true;
}

void vec_gen_2(TCGOpcode opc, TCGType type, unsigned vece, TCGArg r, TCGArg a)
{
    TCGOp *op = tcg_emit_op(opc, 2);
    TCGOP_VECL(op) = type - TCG_TYPE_V64;
    TCGOP_VECE(op) = vece;
    op->args[0] = r;
    op->args[1] = a;
}

void vec_gen_3(TCGOpcode opc, TCGType type, unsigned vece,
               TCGArg r, TCGArg a, TCGArg b)
{
    TCGOp *op = tcg_emit_op(opc, 3);
    TCGOP_VECL(op) = type - TCG_TYPE_V64;
    TCGOP_VECE(op) = vece;
    op->args[0] = r;
    op->args[1] = a;
    op->args[2] = b;
}

void vec_gen_4(TCGOpcode opc, TCGType type, unsigned vece,
               TCGArg r, TCGArg a, TCGArg b, TCGArg c)
{
    TCGOp *op = tcg_emit_op(opc, 4);
    TCGOP_VECL(op) = type - TCG_TYPE_V64;
    TCGOP_VECE(op) = vece;
    op->args[0] = r;
    op->args[1] = a;
    op->args[2] = b;
    op->args[3] = c;
}

/* static void vec_gen_6(TCGOpcode opc, TCGType type, unsigned vece, TCGArg r,
 *                       TCGArg a, TCGArg b, TCGArg c, TCGArg d, TCGArg e)
 * {
 *     TCGOp *op = tcg_emit_op(opc, 6);
 *     TCGOP_VECL(op) = type - TCG_TYPE_V64;
 *     TCGOP_VECE(op) = vece;
 *     op->args[0] = r;
 *     op->args[1] = a;
 *     op->args[2] = b;
 *     op->args[3] = c;
 *     op->args[4] = d;
 *     op->args[5] = e;
 * }  */

static void vec_gen_op2(TCGOpcode opc, unsigned vece, TCGv_vec r, TCGv_vec a)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGType type = rt->base_type;

    /* Must enough inputs for the output.  */
    tcg_debug_assert(at->base_type >= type);
    vec_gen_2(opc, type, vece, temp_arg(rt), temp_arg(at));
}

static void vec_gen_op3(TCGOpcode opc, unsigned vece,
                        TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *bt = tcgv_vec_temp(b);
    TCGType type = rt->base_type;

    /* Must enough inputs for the output.  */
    tcg_debug_assert(at->base_type >= type);
    tcg_debug_assert(bt->base_type >= type);
    vec_gen_3(opc, type, vece, temp_arg(rt), temp_arg(at), temp_arg(bt));
}

void tcg_gen_mov_vec(TCGv_vec r, TCGv_vec a)
{
    if (r != a) {
        tcg_gen_mov_i64_concrete(tcgv_vec_expr_num(r), tcgv_vec_expr_num(a));
        vec_gen_op2(INDEX_op_mov_vec, 0, r, a);
    }
}

void tcg_gen_dupi_vec(unsigned vece, TCGv_vec r, uint64_t a)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    tcg_gen_mov_vec(r, tcg_constant_vec(rt->base_type, vece, a));
}

void tcg_gen_dup_i64_vec(unsigned vece, TCGv_vec r, TCGv_i64 a)
{
    gen_helper_sym_duplicate_value_into_vec(tcgv_vec_expr(r), tcgv_i64_expr(a), tcg_constant_i64(vece), tcg_constant_i64(vec_size(r)));

    TCGArg ri = tcgv_vec_arg(r);
    TCGTemp *rt = arg_temp(ri);
    TCGType type = rt->base_type;

    if (TCG_TARGET_REG_BITS == 64) {
        TCGArg ai = tcgv_i64_arg(a);
        vec_gen_2(INDEX_op_dup_vec, type, vece, ri, ai);
    } else if (vece == MO_64) {
        TCGArg al = tcgv_i32_arg(TCGV_LOW(a));
        TCGArg ah = tcgv_i32_arg(TCGV_HIGH(a));
        vec_gen_3(INDEX_op_dup2_vec, type, MO_64, ri, al, ah);
    } else {
        TCGArg ai = tcgv_i32_arg(TCGV_LOW(a));
        vec_gen_2(INDEX_op_dup_vec, type, vece, ri, ai);
    }
}

void tcg_gen_dup_i32_vec(unsigned vece, TCGv_vec r, TCGv_i32 a)
{
    gen_helper_sym_duplicate_value_into_vec(tcgv_vec_expr(r), tcgv_i32_expr(a), tcg_constant_i64(vece), tcg_constant_i64(vec_size(r)));

    TCGArg ri = tcgv_vec_arg(r);
    TCGArg ai = tcgv_i32_arg(a);
    TCGTemp *rt = arg_temp(ri);
    TCGType type = rt->base_type;

    vec_gen_2(INDEX_op_dup_vec, type, vece, ri, ai);
}

void tcg_gen_dup_mem_vec(unsigned vece, TCGv_vec r, TCGv_ptr b,
                         tcg_target_long ofs)
{
    gen_helper_sym_load_and_duplicate_into_vec(
            tcgv_vec_expr(r),
            b,
            tcg_constant_i64(ofs),
            tcg_constant_i64(vec_size(r)),
            tcg_constant_i64(vece)
    );

    TCGArg ri = tcgv_vec_arg(r);
    TCGArg bi = tcgv_ptr_arg(b);
    TCGTemp *rt = arg_temp(ri);
    TCGType type = rt->base_type;

    vec_gen_3(INDEX_op_dupm_vec, type, vece, ri, bi, ofs);
}

static void vec_gen_ldst(TCGOpcode opc, TCGv_vec r, TCGv_ptr b, TCGArg o)
{
    TCGArg ri = tcgv_vec_arg(r);
    TCGArg bi = tcgv_ptr_arg(b);
    TCGTemp *rt = arg_temp(ri);
    TCGType type = rt->base_type;

    vec_gen_3(opc, type, 0, ri, bi, o);
}

void tcg_gen_ld_vec(TCGv_vec r, TCGv_ptr b, TCGArg o)
{

    gen_helper_sym_load_host_vec(
                tcgv_vec_expr(r),
                b,
                tcg_constant_i64(o),
                tcg_constant_i64(vec_size(r) / 8)
            );

    vec_gen_ldst(INDEX_op_ld_vec, r, b, o);
}

void tcg_gen_st_vec(TCGv_vec r, TCGv_ptr b, TCGArg o)
{
    gen_helper_sym_store_host(
                tcgv_vec_expr(r),
                b,
                tcg_constant_i64(o),
                tcg_constant_i64(vec_size(r) / 8)
            );

    vec_gen_ldst(INDEX_op_st_vec, r, b, o);
}

void tcg_gen_stl_vec(TCGv_vec r, TCGv_ptr b, TCGArg o, TCGType low_type)
{
    TCGArg ri = tcgv_vec_arg(r);
    TCGArg bi = tcgv_ptr_arg(b);
    TCGTemp *rt = arg_temp(ri);
    TCGType type = rt->base_type;

    tcg_debug_assert(low_type >= TCG_TYPE_V64);
    tcg_debug_assert(low_type <= type);

    uint64_t length;

    switch (low_type) {
        case TCG_TYPE_V64:
            length = 8;
            break;
        case TCG_TYPE_V128:
            length = 16;
            break;
        case TCG_TYPE_V256:
            length = 32;
            break;
        default:
            g_assert_not_reached();
    }

    gen_helper_sym_store_host(tcgv_vec_expr(r), b, tcg_constant_i64(o), tcg_constant_i64(length));

    vec_gen_3(INDEX_op_st_vec, low_type, 0, ri, bi, o);
}

void tcg_gen_and_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_and_vec);
    vec_gen_op3(INDEX_op_and_vec, 0, r, a, b);
}

void tcg_gen_or_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_or_vec);
    vec_gen_op3(INDEX_op_or_vec, 0, r, a, b);
}

void tcg_gen_xor_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_xor_vec);
    vec_gen_op3(INDEX_op_xor_vec, 0, r, a, b);
}

void tcg_gen_andc_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{

   /*
    * TODO (SymQEMU):
    * This instruction is not instrumented yet.
    * For now, we make sure that alternative instructions below, which are instrumented, are always used.
    * Directly instrumenting this instruction would improve performance of SymQEMU.
    */


   /* if (TCG_TARGET_HAS_andc_vec) {
        vec_gen_op3(INDEX_op_andc_vec, 0, r, a, b);
    } else { */
        TCGv_vec t = tcg_temp_new_vec_matching(r);
        tcg_gen_not_vec(0, t, b);
        tcg_gen_and_vec(0, r, a, t);
        tcg_temp_free_vec(t);
   /* } */
}

void tcg_gen_orc_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{

    /*
     * TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */


    /* if (TCG_TARGET_HAS_orc_vec) {
        vec_gen_op3(INDEX_op_orc_vec, 0, r, a, b);
    } else { */
        TCGv_vec t = tcg_temp_new_vec_matching(r);
        tcg_gen_not_vec(0, t, b);
        tcg_gen_or_vec(0, r, a, t);
        tcg_temp_free_vec(t);
    /* } */
}

void tcg_gen_nand_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{

    /*
     * TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */

    /* if (TCG_TARGET_HAS_nand_vec) {
        vec_gen_op3(INDEX_op_nand_vec, 0, r, a, b);
    } else { */
        tcg_gen_and_vec(0, r, a, b);
        tcg_gen_not_vec(0, r, r);
    /* } */
}

void tcg_gen_nor_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{

    /* TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */


    /* if (TCG_TARGET_HAS_nor_vec) {
        vec_gen_op3(INDEX_op_nor_vec, 0, r, a, b);
    } else { */
        tcg_gen_or_vec(0, r, a, b);
        tcg_gen_not_vec(0, r, r);
    /* } */
}

void tcg_gen_eqv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{

    /* TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU. */
    

    /* if (TCG_TARGET_HAS_eqv_vec) {
        vec_gen_op3(INDEX_op_eqv_vec, 0, r, a, b);
    } else { */
        tcg_gen_xor_vec(0, r, a, b);
        tcg_gen_not_vec(0, r, r);
    /* } */
}

/* static bool do_op2(unsigned vece, TCGv_vec r, TCGv_vec a, TCGOpcode opc)
 * {
 *     TCGTemp *rt = tcgv_vec_temp(r);
 *     TCGTemp *at = tcgv_vec_temp(a);
 *     TCGArg ri = temp_arg(rt);
 *     TCGArg ai = temp_arg(at);
 *     TCGType type = rt->base_type;
 *     int can;
 * 
 *     tcg_debug_assert(at->base_type >= type);
 *     tcg_assert_listed_vecop(opc);
 *     can = tcg_can_emit_vec_op(opc, type, vece);
 *     if (can > 0) {
 *         vec_gen_2(opc, type, vece, ri, ai);
 *     } else if (can < 0) {
 *         const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
 *         tcg_expand_vec_op(opc, type, vece, ri, ai);
 *         tcg_swap_vecop_list(hold_list);
 *     } else {
 *         return false;
 *     }
 *     return true;
 * }
 */

void tcg_gen_not_vec(unsigned vece, TCGv_vec r, TCGv_vec a)
{
    if (TCG_TARGET_HAS_not_vec) {
        vec_gen_op2(INDEX_op_not_vec, 0, r, a);
    } else {
        tcg_gen_xor_vec(0, r, a, tcg_constant_vec_matching(r, 0, -1));
    }
}

void tcg_gen_neg_vec(unsigned vece, TCGv_vec r, TCGv_vec a)
{
    const TCGOpcode *hold_list;

    tcg_assert_listed_vecop(INDEX_op_neg_vec);
    hold_list = tcg_swap_vecop_list(NULL);

   
    /* TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */
    

    /*if (!TCG_TARGET_HAS_neg_vec || !do_op2(vece, r, a, INDEX_op_neg_vec)) {*/
        tcg_gen_sub_vec(vece, r, tcg_constant_vec_matching(r, vece, 0), a);
    /*}*/
    tcg_swap_vecop_list(hold_list);
}

void tcg_gen_abs_vec(unsigned vece, TCGv_vec r, TCGv_vec a)
{
    const TCGOpcode *hold_list;

    tcg_assert_listed_vecop(INDEX_op_abs_vec);
    hold_list = tcg_swap_vecop_list(NULL);


    /*
     * TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */


    /*if (!do_op2(vece, r, a, INDEX_op_abs_vec)) {*/
        TCGType type = tcgv_vec_temp(r)->base_type;
        TCGv_vec t = tcg_temp_new_vec(type);

        tcg_debug_assert(tcg_can_emit_vec_op(INDEX_op_sub_vec, type, vece));
        if (tcg_can_emit_vec_op(INDEX_op_smax_vec, type, vece) > 0) {
            tcg_gen_neg_vec(vece, t, a);
            tcg_gen_smax_vec(vece, r, a, t);
        } else {
            if (tcg_can_emit_vec_op(INDEX_op_sari_vec, type, vece) > 0) {
                tcg_gen_sari_vec(vece, t, a, (8 << vece) - 1);
            } else {
                tcg_gen_cmp_vec(TCG_COND_LT, vece, t, a,
                                tcg_constant_vec(type, vece, 0));
            }
            tcg_gen_xor_vec(vece, r, a, t);
            tcg_gen_sub_vec(vece, r, r, t);
        }

        tcg_temp_free_vec(t);
    /*}*/
    tcg_swap_vecop_list(hold_list);
}

static void do_shifti(TCGOpcode opc, unsigned vece,
                      TCGv_vec r, TCGv_vec a, int64_t i)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGArg ri = temp_arg(rt);
    TCGArg ai = temp_arg(at);
    TCGType type = rt->base_type;
    int can;

    tcg_debug_assert(at->base_type == type);
    tcg_debug_assert(i >= 0 && i < (8 << vece));
    tcg_assert_listed_vecop(opc);

    if (i == 0) {
        tcg_gen_mov_vec(r, a);
        return;
    }

    can = tcg_can_emit_vec_op(opc, type, vece);
    if (can > 0) {
        vec_gen_3(opc, type, vece, ri, ai, i);
    } else {
        /* We leave the choice of expansion via scalar or vector shift
           to the target.  Often, but not always, dupi can feed a vector
           shift easier than a scalar.  */
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        tcg_debug_assert(can < 0);
        tcg_expand_vec_op(opc, type, vece, ri, ai, i);
        tcg_swap_vecop_list(hold_list);
    }
}

void tcg_gen_shli_vec(unsigned vece, TCGv_vec r, TCGv_vec a, int64_t i)
{
    vec_int32_op_instrumentation(vece, r, a, tcg_constant_i32(i), gen_helper_sym_shift_left_vec_int32);
    do_shifti(INDEX_op_shli_vec, vece, r, a, i);
}

void tcg_gen_shri_vec(unsigned vece, TCGv_vec r, TCGv_vec a, int64_t i)
{
    vec_int32_op_instrumentation(vece, r, a, tcg_constant_i32(i), gen_helper_sym_logical_shift_right_vec_int32);
    do_shifti(INDEX_op_shri_vec, vece, r, a, i);
}

void tcg_gen_sari_vec(unsigned vece, TCGv_vec r, TCGv_vec a, int64_t i)
{
    vec_int32_op_instrumentation(vece, r, a, tcg_constant_i32(i), gen_helper_sym_arithmetic_shift_right_vec_int32);
    do_shifti(INDEX_op_sari_vec, vece, r, a, i);
}

void tcg_gen_rotli_vec(unsigned vece, TCGv_vec r, TCGv_vec a, int64_t i)
{
    vec_int32_op_instrumentation(vece, r, a, tcg_constant_i32(i), gen_helper_sym_rotate_left_vec_int32);
    do_shifti(INDEX_op_rotli_vec, vece, r, a, i);
}

void tcg_gen_rotri_vec(unsigned vece, TCGv_vec r, TCGv_vec a, int64_t i)
{
    int bits = 8 << vece;
    tcg_debug_assert(i >= 0 && i < bits);
    vec_int32_op_instrumentation(vece, r, a, tcg_constant_i32(i), gen_helper_sym_rotate_right_vec_int32);
    do_shifti(INDEX_op_rotli_vec, vece, r, a, -i & (bits - 1));
}

void tcg_gen_cmp_vec(TCGCond cond, unsigned vece,
                     TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *bt = tcgv_vec_temp(b);
    TCGArg ri = temp_arg(rt);
    TCGArg ai = temp_arg(at);
    TCGArg bi = temp_arg(bt);
    TCGType type = rt->base_type;
    int can;

    /* gen_helper_sym_cmp_vec below needs the concrete value of a and b.
     * However, if r designates the same TCGTemp as a or b, the execution of the concrete cmp operation
     * will overwrite the value of a or b. Therefore, we need to store the values of a and b
     * *before* the concrete cmp operation is executed.
     */
    TCGv_ptr buffer_address_a = store_vector_in_memory(a);
    TCGv_ptr buffer_address_b = store_vector_in_memory(b);

    tcg_debug_assert(at->base_type >= type);
    tcg_debug_assert(bt->base_type >= type);
    tcg_assert_listed_vecop(INDEX_op_cmp_vec);
    can = tcg_can_emit_vec_op(INDEX_op_cmp_vec, type, vece);
    if (can > 0) {
        vec_gen_4(INDEX_op_cmp_vec, type, vece, ri, ai, bi, cond);
    } else {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        tcg_debug_assert(can < 0);
        tcg_expand_vec_op(INDEX_op_cmp_vec, type, vece, ri, ai, bi, cond);
        tcg_swap_vecop_list(hold_list);
    }

    TCGv_ptr buffer_address_r = store_vector_in_memory(r);
    int size_a = vec_size(a);
    int size_b = vec_size(b);
    int size_r = vec_size(r);

    g_assert(size_a == size_b && size_b == size_r);

    gen_helper_sym_cmp_vec(
            tcgv_vec_expr(r),
            tcg_env,
            buffer_address_a,
            tcgv_vec_expr(a),
            buffer_address_b,
            tcgv_vec_expr(b),
            tcg_constant_i32(cond),
            buffer_address_r,
            tcg_constant_i64(size_a),
            tcg_constant_i64(vece)
    );

    gen_helper_free(buffer_address_a);
    gen_helper_free(buffer_address_b);
    gen_helper_free(buffer_address_r);

}

static bool do_op3(unsigned vece, TCGv_vec r, TCGv_vec a,
                   TCGv_vec b, TCGOpcode opc)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *bt = tcgv_vec_temp(b);
    TCGArg ri = temp_arg(rt);
    TCGArg ai = temp_arg(at);
    TCGArg bi = temp_arg(bt);
    TCGType type = rt->base_type;
    int can;

    tcg_debug_assert(at->base_type >= type);
    tcg_debug_assert(bt->base_type >= type);
    tcg_assert_listed_vecop(opc);
    can = tcg_can_emit_vec_op(opc, type, vece);
    if (can > 0) {
        vec_gen_3(opc, type, vece, ri, ai, bi);
    } else if (can < 0) {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        tcg_expand_vec_op(opc, type, vece, ri, ai, bi);
        tcg_swap_vecop_list(hold_list);
    } else {
        return false;
    }
    return true;
}

static void do_op3_nofail(unsigned vece, TCGv_vec r, TCGv_vec a,
                          TCGv_vec b, TCGOpcode opc)
{
    bool ok = do_op3(vece, r, a, b, opc);
    tcg_debug_assert(ok);
}

void tcg_gen_add_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_add_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_add_vec);
}

void tcg_gen_sub_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_sub_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_sub_vec);
}

void tcg_gen_mul_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_mul_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_mul_vec);
}

void tcg_gen_ssadd_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_signed_saturating_add_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_ssadd_vec);
}

void tcg_gen_usadd_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_unsigned_saturating_add_vec);

    if (!do_op3(vece, r, a, b, INDEX_op_usadd_vec)) {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        TCGv_vec t = tcg_temp_new_vec_matching(r);

        /* usadd(a, b) = min(a, ~b) + b */
        tcg_gen_not_vec(vece, t, b);
        tcg_gen_umin_vec(vece, t, t, a);
        tcg_gen_add_vec(vece, r, t, b);

        tcg_temp_free_vec(t);
        tcg_swap_vecop_list(hold_list);
    }
}

void tcg_gen_sssub_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_signed_saturating_sub_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_sssub_vec);
}

void tcg_gen_ussub_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_unsigned_saturating_sub_vec);

    if (!do_op3(vece, r, a, b, INDEX_op_ussub_vec)) {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        TCGv_vec t = tcg_temp_new_vec_matching(r);

        /* ussub(a, b) = max(a, b) - b */
        tcg_gen_umax_vec(vece, t, a, b);
        tcg_gen_sub_vec(vece, r, t, b);

        tcg_temp_free_vec(t);
        tcg_swap_vecop_list(hold_list);
    }
}

static void do_minmax(unsigned vece, TCGv_vec r, TCGv_vec a,
                      TCGv_vec b, TCGOpcode opc, TCGCond cond)
{
    /*
     * gen_helper_sym_ternary_vec below needs the concrete values of a and b.
     * However, if r designates the same TCGTemp as a or b, the execution of the concrete min/max operation
     * will overwrite the value of a or b. Therefore, we need to store the values of a and b
     * *before* the concrete cmp operation is executed.
     */
    TCGv_ptr buffer_address_a = store_vector_in_memory(a);
    TCGv_ptr buffer_address_b = store_vector_in_memory(b);

    /* Note that, if the if below is entered, an infinite recursion occurs.
     * This is due to the fact that tcg_expand_vec_op, which is called by several functions in the present file,
     * internally leads to calls to min / max functions.
     * For this reason, here we have to provide an explicit instrumentation of the min / max functions instead
     * of forcing the instruction to be generated with other instrumented functions, as done in several other cases.
     */
    if (!do_op3(vece, r, a, b, opc)) {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        tcg_gen_cmpsel_vec(cond, vece, r, a, b, a, b);
        tcg_swap_vecop_list(hold_list);
    }

    TCGv_ptr buffer_address_r = store_vector_in_memory(r);
    int size_a = vec_size(a);
    int size_b = vec_size(b);
    int size_r = vec_size(r);

    g_assert(size_a == size_b && size_b == size_r);

    gen_helper_sym_ternary_vec(
            tcgv_vec_expr(r),
            tcg_env,
            buffer_address_a,
            tcgv_vec_expr(a),
            buffer_address_b,
            tcgv_vec_expr(b),
            tcg_constant_i32(cond),
            buffer_address_r,
            tcg_constant_i64(size_a),
            tcg_constant_i64(vece)
    );

    gen_helper_free(buffer_address_a);
    gen_helper_free(buffer_address_b);
    gen_helper_free(buffer_address_r);
}

void tcg_gen_smin_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    do_minmax(vece, r, a, b, INDEX_op_smin_vec, TCG_COND_LT);
}

void tcg_gen_umin_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    do_minmax(vece, r, a, b, INDEX_op_umin_vec, TCG_COND_LTU);
}

void tcg_gen_smax_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    do_minmax(vece, r, a, b, INDEX_op_smax_vec, TCG_COND_GT);
}

void tcg_gen_umax_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    do_minmax(vece, r, a, b, INDEX_op_umax_vec, TCG_COND_GTU);
}

void tcg_gen_shlv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_shift_left_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_shlv_vec);
}

void tcg_gen_shrv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_logical_shift_right_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_shrv_vec);
}

void tcg_gen_sarv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_arithmetic_shift_right_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_sarv_vec);
}

void tcg_gen_rotlv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_rotate_left_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_rotlv_vec);
}

void tcg_gen_rotrv_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_vec b)
{
    vec_vec_op_instrumentation(vece, r, a, b, gen_helper_sym_rotate_right_vec);
    do_op3_nofail(vece, r, a, b, INDEX_op_rotrv_vec);
}

static void do_shifts(unsigned vece, TCGv_vec r, TCGv_vec a,
                      TCGv_i32 s, TCGOpcode opc)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *st = tcgv_i32_temp(s);
    TCGArg ri = temp_arg(rt);
    TCGArg ai = temp_arg(at);
    TCGArg si = temp_arg(st);
    TCGType type = rt->base_type;
    int can;

    tcg_debug_assert(at->base_type >= type);
    tcg_assert_listed_vecop(opc);
    can = tcg_can_emit_vec_op(opc, type, vece);
    if (can > 0) {
        vec_gen_3(opc, type, vece, ri, ai, si);
    } else if (can < 0) {
        const TCGOpcode *hold_list = tcg_swap_vecop_list(NULL);
        tcg_expand_vec_op(opc, type, vece, ri, ai, si);
        tcg_swap_vecop_list(hold_list);
    } else {
        g_assert_not_reached();
    }
}

void tcg_gen_shls_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_i32 b)
{
    vec_int32_op_instrumentation(vece, r, a, b, gen_helper_sym_shift_left_vec_int32);
    do_shifts(vece, r, a, b, INDEX_op_shls_vec);
}

void tcg_gen_shrs_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_i32 b)
{
    vec_int32_op_instrumentation(vece, r, a, b, gen_helper_sym_logical_shift_right_vec_int32);
    do_shifts(vece, r, a, b, INDEX_op_shrs_vec);
}

void tcg_gen_sars_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_i32 b)
{
    vec_int32_op_instrumentation(vece, r, a, b, gen_helper_sym_arithmetic_shift_right_vec_int32);
    do_shifts(vece, r, a, b, INDEX_op_sars_vec);
}

void tcg_gen_rotls_vec(unsigned vece, TCGv_vec r, TCGv_vec a, TCGv_i32 s)
{
    vec_int32_op_instrumentation(vece, r, a, s, gen_helper_sym_rotate_left_vec_int32);
    do_shifts(vece, r, a, s, INDEX_op_rotls_vec);
}

void tcg_gen_bitsel_vec(unsigned vece, TCGv_vec r, TCGv_vec a,
                        TCGv_vec b, TCGv_vec c)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *bt = tcgv_vec_temp(b);
    TCGTemp *ct = tcgv_vec_temp(c);
    TCGType type = rt->base_type;

    tcg_debug_assert(at->base_type >= type);
    tcg_debug_assert(bt->base_type >= type);
    tcg_debug_assert(ct->base_type >= type);


    /*
     * TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */


    /*
     *  if (TCG_TARGET_HAS_bitsel_vec) {
     *     vec_gen_4(INDEX_op_bitsel_vec, type, MO_8,
     *               temp_arg(rt), temp_arg(at), temp_arg(bt), temp_arg(ct));
     * } else {
     */
        TCGv_vec t = tcg_temp_new_vec(type);
        tcg_gen_and_vec(MO_8, t, a, b);
        tcg_gen_andc_vec(MO_8, r, c, a);
        tcg_gen_or_vec(MO_8, r, r, t);
        tcg_temp_free_vec(t);
    /* } */
}

void tcg_gen_cmpsel_vec(TCGCond cond, unsigned vece, TCGv_vec r,
                        TCGv_vec a, TCGv_vec b, TCGv_vec c, TCGv_vec d)
{
    TCGTemp *rt = tcgv_vec_temp(r);
    TCGTemp *at = tcgv_vec_temp(a);
    TCGTemp *bt = tcgv_vec_temp(b);
    TCGTemp *ct = tcgv_vec_temp(c);
    TCGTemp *dt = tcgv_vec_temp(d);
    /*
     * TCGArg ri = temp_arg(rt);
     * TCGArg ai = temp_arg(at);
     * TCGArg bi = temp_arg(bt);
     * TCGArg ci = temp_arg(ct);
     * TCGArg di = temp_arg(dt);
     */
    TCGType type = rt->base_type;
    const TCGOpcode *hold_list;
    /* int can; */

    tcg_debug_assert(at->base_type >= type);
    tcg_debug_assert(bt->base_type >= type);
    tcg_debug_assert(ct->base_type >= type);
    tcg_debug_assert(dt->base_type >= type);

    tcg_assert_listed_vecop(INDEX_op_cmpsel_vec);
    hold_list = tcg_swap_vecop_list(NULL);


    /*
     * TODO (SymQEMU):
     * This instruction is not instrumented yet.
     * For now, we make sure that alternative instructions below, which are instrumented, are always used.
     * Directly instrumenting this instruction would improve performance of SymQEMU.
     */

    /*
     * can = tcg_can_emit_vec_op(INDEX_op_cmpsel_vec, type, vece);
     *
     * if (can > 0) {
     *     vec_gen_6(INDEX_op_cmpsel_vec, type, vece, ri, ai, bi, ci, di, cond);
     * } else if (can < 0) {
     *     tcg_expand_vec_op(INDEX_op_cmpsel_vec, type, vece,
     *                       ri, ai, bi, ci, di, cond);
     * } else {
     */
        TCGv_vec t = tcg_temp_new_vec(type);
        tcg_gen_cmp_vec(cond, vece, t, a, b);
        tcg_gen_bitsel_vec(vece, r, t, c, d);
        tcg_temp_free_vec(t);
    /* } */
    tcg_swap_vecop_list(hold_list);
}
