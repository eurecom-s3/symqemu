/*
 * This file is part of SymQEMU.
 * It contains symbolic helper functions for vector operations.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/translation-block.h"
#include "accel/tcg/tcg-runtime-sym-common.h"

#define HELPER_H  "accel/tcg/tcg-runtime-sym-vec.h"

#include "exec/helper-info.c.inc"

#undef  HELPER_H

/* Include the symbolic backend, using void* as expression type. */

#define SymExpr void*

#include "RuntimeCommon.h"

void *HELPER(malloc)(uint64_t size) {
    return malloc(size);
}

void HELPER(free)(void *ptr) {
    free(ptr);
}

/*
 * Splits an expression into multiple expressions of equal size.
 *
 * The expressions are stored in little endian order : result[0] is the least significant element of the original expression,
 * result[element_count - 1] is the most significant element.
 *
 * Args
 *      expression : the expression to split
 *      element_count : number of expressions to split into
 *      element_size : size of each result expression in bits
 *      result : array of size element_count to store the result
 */
static void split_expression(void *expression, uint64_t element_count, uint64_t element_size, void *result[]) {
    g_assert(_sym_bits_helper(expression) % element_count == 0);
    for (uint64_t i = 0; i < element_count; i++) {
        result[i] = _sym_extract_helper(expression, (i + 1) * element_size - 1, i * element_size);
    }
}

/* Pairwise applies a binary operation on two arrays of expressions and concatenates the result into a single expression.
 *
 * The concatenation is done in little endian order : symbolic_operation(args1[0], args2[0]) is the least significant
 * element of the result, symbolic_operation(args1[element_count - 1], args2[element_count - 1]) is the most significant.
 *
 * Args
 *      symbolic_operation : function for the symbolic operation applied on each pair of elements
 *      args1 : first array of symbols
 *      args2 : second array of symbols
 *      element_count : number of elements in each array
 * Returns
 *      An expression that represents the concatenated results
 */
static void *apply_op_and_merge(
        void *(*symbolic_operation)(void *, void *),
        void *args1[], void *args2[],
        uint64_t element_count
) {
    void *merged_result = symbolic_operation(args1[0], args2[0]);
    for (uint64_t i = 1; i < element_count; i++) {
        g_assert(_sym_bits_helper(args1[0]) == _sym_bits_helper(args1[i]));
        merged_result = _sym_concat_helper(symbolic_operation(args1[i], args2[i]), merged_result);
    }
    return merged_result;
}

static uint64_t vece_element_size(uint64_t vece) {
    /* see definition of vece here https://www.qemu.org/docs/master/devel/tcg-ops.html#host-vector-operations */
    return 8 << vece;
}

/*
 * Builds a symbolic expression for an SIMD operation on two vector operands.
 * Args
 *      arg1_concrete, arg2_concrete : pointers to buffers that store the concrete values of the input vector operands
 *      arg1_symbolic, arg2_symbolic : symbolic expressions of the input vector operands
 *      vector_size : size of the vectors in bits
 *      vece : element size in bits = 8 * 2^vece
 *      symbolic_operation : function for the symbolic operation applied on each element of the vectors
 * Returns
 *      A symbol expression that represents the output of the SIMD operation
 */
static void *build_expression_for_vector_vector_op(
        void *arg1_concrete, void *arg1_symbolic,
        void *arg2_concrete, void *arg2_symbolic,
        uint64_t vector_size, uint64_t vece,
        void *(*symbolic_operation)(void *, void *)
) {
    uint64_t element_size = vece_element_size(vece);

    g_assert(vector_size % element_size == 0);
    uint64_t element_count = vector_size / element_size;

    static void** arg1_elts      = NULL;
    static void** arg2_elts      = NULL;

    static uint64_t arg_len = 0;

    uint64_t arg_len_required = element_count * sizeof(void*);
    if (arg_len < arg_len_required) {
        arg1_elts = g_realloc(arg1_elts, arg_len_required);
        arg2_elts = g_realloc(arg2_elts, arg_len_required);

        arg_len = arg_len_required;
    }

    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    g_assert(element_size <= vector_size);

    if (arg1_symbolic == NULL && arg2_symbolic == NULL) {
        return NULL;
    }

    if (arg1_symbolic == NULL) {
        arg1_symbolic = _sym_build_integer_from_buffer(arg1_concrete, vector_size);
    }

    if (arg2_symbolic == NULL) {
        arg2_symbolic = _sym_build_integer_from_buffer(arg2_concrete, vector_size);
    }

    g_assert(_sym_bits_helper(arg1_symbolic) == _sym_bits_helper(arg2_symbolic) &&
             _sym_bits_helper(arg1_symbolic) == vector_size);

    split_expression(arg1_symbolic, element_count, element_size, arg1_elts);
    split_expression(arg2_symbolic, element_count, element_size, arg2_elts);

    void *result = apply_op_and_merge(symbolic_operation, arg1_elts, arg2_elts, element_count);
    g_assert(_sym_bits_helper(result) == vector_size);
    return result;
}

/*
 * Builds a symbolic expression for an SIMD operation on a vector operand and an integer operand.
 *
 * The operation is performed like if arg2 was duplicated into a vector of the same size as arg1.
 *
 * Args
 *      arg1_concrete : pointer to a buffer that stores the concrete value of the input vector operand
 *      arg1_symbolic : symbolic expression of the input vector operand
 *      arg2_concrete: concrete value of the input integer operand
 *      arg2_symbolic : symbolic expression of the input integer operand
 *      vector_size : size of the vector in bits
 *      vece: element size in bits = 8 * 2^vece
 *      symbolic_operation : function for the symbolic operation applied on each element of the vector
 * Returns
 *    A symbol expression that represents the output of the SIMD operation
 */
static void *
build_expression_for_vector_int32_op(
        void *arg1_concrete, void *arg1_symbolic,
        uint32_t arg2_concrete, void *arg2_symbolic,
        uint64_t vector_size, uint64_t vece,
        void *(*symbolic_operation)(void *, void *)
) {
    uint64_t element_size = vece_element_size(vece);

    g_assert(vector_size % element_size == 0);
    uint64_t element_count = vector_size / element_size;

    static void** arg1_elts      = NULL;
    static void** arg2_elts      = NULL;

    static uint64_t arg_len = 0;

    uint64_t arg_len_required = element_count * sizeof(void*);
    if (arg_len < arg_len_required) {
        arg1_elts = g_realloc(arg1_elts, arg_len_required);
        arg2_elts = g_realloc(arg2_elts, arg_len_required);

        arg_len = arg_len_required;
    }

    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    g_assert(element_size <= vector_size);

    if (arg1_symbolic == NULL && arg2_symbolic == NULL) {
        return NULL;
    }

    if (arg1_symbolic == NULL) {
        arg1_symbolic = _sym_build_integer_from_buffer(arg1_concrete, vector_size);
    }

    if (arg2_symbolic == NULL) {
        arg2_symbolic = _sym_build_integer(arg2_concrete, 32);
    }

    g_assert(_sym_bits_helper(arg1_symbolic) == vector_size);
    g_assert(_sym_bits_helper(arg2_symbolic) == 32);

    if (element_size != 32) {
        arg2_symbolic = _sym_build_zext(arg2_symbolic, element_size - 32);
    }

    split_expression(arg1_symbolic, element_count, element_size, arg1_elts);
    for (uint64_t i = 0; i < element_count; i++) {
        arg2_elts[i] = arg2_symbolic;
    }

    void *result = apply_op_and_merge(symbolic_operation, arg1_elts, arg2_elts, element_count);
    g_assert(_sym_bits_helper(result) == vector_size);
    return result;
}

void *HELPER(sym_and_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_and);
}

void *HELPER(sym_or_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_or);
}

void *HELPER(sym_xor_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_xor);
}

void *HELPER(sym_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_add);
}

void *HELPER(sym_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_sub);
}

void *HELPER(sym_mul_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_mul);
}

void *HELPER(sym_signed_saturating_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                            uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_sadd_sat);
}

void *HELPER(sym_signed_saturating_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                            uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_ssub_sat);
}

void *HELPER(sym_unsigned_saturating_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                              uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_uadd_sat);
}

void *HELPER(sym_unsigned_saturating_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                              uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_usub_sat);
}

void *
HELPER(sym_shift_left_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_shift_left);
}

void *HELPER(sym_logical_shift_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                          uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,
                                                 _sym_build_logical_shift_right);
}

void *HELPER(sym_arithmetic_shift_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size,
                                             uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,
                                                 _sym_build_arithmetic_shift_right);
}

void *
HELPER(sym_rotate_left_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, sym_rotate_left);
}

void *
HELPER(sym_rotate_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_expression_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,sym_rotate_right);
}

void *HELPER(sym_shift_left_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size,
                                       uint64_t vece) {
    return build_expression_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_shift_left);
}

void *
HELPER(sym_logical_shift_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size,
                                          uint64_t vece) {
    return build_expression_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,
                                                _sym_build_logical_shift_right);
}

void *
HELPER(sym_arithmetic_shift_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size,
                                             uint64_t vece) {
    return build_expression_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,
                                                _sym_build_arithmetic_shift_right);
}

void *HELPER(sym_rotate_left_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size,
                                        uint64_t vece) {
    return build_expression_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece,sym_rotate_left);
}

void *HELPER(sym_rotate_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size,
                                         uint64_t vece) {
    return build_expression_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, sym_rotate_right);
}

/*
 * Symbolic equivalent of creating a vector by duplicating a value.
 *
 * value_expression is resized to match vece before being duplicated.
 */
void *HELPER(sym_duplicate_value_into_vec)(void *value_expression, uint64_t vector_size, uint64_t vece) {
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (value_expression == NULL) {
        return NULL;
    }

    g_assert(_sym_bits_helper(value_expression) == 32 || _sym_bits_helper(value_expression) == 64);

    void *resized_value_expr;

    if (_sym_bits_helper(value_expression) == element_size) {
        resized_value_expr = value_expression;
    } else if (_sym_bits_helper(value_expression) < element_size) {
        resized_value_expr = _sym_build_zext(value_expression, element_size);
    } else {
        resized_value_expr = _sym_extract_helper(value_expression, element_size - 1, 0);
    }

    uint64_t element_count = vector_size / element_size;
    void *result_expression = resized_value_expr;
    for (uint64_t i = 1; i < element_count; i++) {
        result_expression = _sym_concat_helper(resized_value_expr, result_expression);
    }

    g_assert(_sym_bits_helper(result_expression) == vector_size);
    return result_expression;
}

/*
 * Symbolic equivalent of a loading a value from memory and duplicating it into a vector.
 */
void *HELPER(sym_load_and_duplicate_into_vec)(
        void *load_address,
        uint64_t address_offset,
        uint64_t vector_size, uint64_t vece
) {
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    void *value_expr = helper_sym_load_host_vec(load_address, address_offset, element_size / 8);
    g_assert(value_expr == NULL || _sym_bits_helper(value_expr) == element_size);

    return helper_sym_duplicate_value_into_vec(value_expr, vector_size, vece);
}

/*
 * Computes the address of an element in a vector.
 */
static void *element_address(
        void *concrete_vector,
        uint64_t element_index,
        uint64_t element_size,
        uint64_t vector_size
) {
    void *result = concrete_vector + element_index * element_size / 8;
    g_assert(result + element_size <= concrete_vector + vector_size);
    return result;
}

/*
TODO:
    Currently, `helper_sym_cmp_vec` pushes path constraints and concretizes its result.
    Returning an expression that represents the result of the comparison, instead of pushing path constraints,
    may give better results.
*/

/*
 * Symbolic equivalent of an SIMD comparison operation, where true is represented by -1 and false by 0.
 *
 * This function performs the symbolic equivalent of computing an output vector of the form :
 * result[i] = arg1[i] <comparison_operator> arg2[i] ? -1 : 0
 *
 * For each element, the concrete result is used to determine if the comparison was true or false,
 * and path constraints are pushed accordingly.
 *
 * Args
 *   arg1_concrete, arg2_concrete : pointers to buffers that store the concrete values of the input vector operands
 *   arg1_symbolic, arg2_symbolic : symbolic expressions of the input vector operands
 *   comparison_operator : enum value that represents a comparison operator like <, >, ==, etc.
 *   result_concrete : pointer to a buffer that stores the concrete value of the result vector, as computed by the
 *       concrete execution of the SIMD operation
 *   vector_size : size of the vectors in bits
 *   vece : element size in bits = 8 * 2^vece
 *
 */
void *HELPER(sym_cmp_vec)(
        CPUArchState *env,
        void *arg1_concrete, void *arg1_symbolic,
        void *arg2_concrete, void *arg2_symbolic,
        /* TCGCond */ uint32_t comparison_operator, void *result_concrete,
        uint64_t vector_size, uint64_t vece
) {
    uint64_t element_size = vece_element_size(vece);

    g_assert(vector_size % element_size == 0);
    uint64_t element_count = vector_size / element_size;

    static void** arg1_elts      = NULL;
    static void** arg2_elts      = NULL;

    static uint64_t arg_len = 0;

    uint64_t arg_len_required = element_count * sizeof(void*);
    if (arg_len < arg_len_required) {
        arg1_elts = g_realloc(arg1_elts, arg_len_required);
        arg2_elts = g_realloc(arg2_elts, arg_len_required);

        arg_len = arg_len_required;
    }

    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (arg1_symbolic == NULL && arg2_symbolic == NULL) {
        return NULL;
    }

    if (arg1_symbolic == NULL) {
        arg1_symbolic = _sym_build_integer_from_buffer(arg1_concrete, vector_size);
    }

    if (arg2_symbolic == NULL) {
        arg2_symbolic = _sym_build_integer_from_buffer(arg2_concrete, vector_size);
    }

    g_assert(_sym_bits_helper(arg1_symbolic) == _sym_bits_helper(arg2_symbolic) &&
             _sym_bits_helper(arg1_symbolic) == vector_size);

    split_expression(arg1_symbolic, element_count, element_size, arg1_elts);
    split_expression(arg2_symbolic, element_count, element_size, arg2_elts);

    for (uint64_t i = 0; i < element_count; i++) {
        /* For each element, the comparison was true iff the element of the result is equal to -1.
        * Here it is ok to take the first byte of the element, because the result is either -1 or 0. */
        uint8_t is_taken = *(uint8_t *) element_address(result_concrete, i, element_size, vector_size);
        build_and_push_path_constraint(
                env,
                arg1_elts[i],
                arg2_elts[i],
                comparison_operator,
                is_taken
        );

    }

    /* Concretize the result, as path constraints for its value have been pushed. */
    return NULL;
}

/*
TODO:
    Currently, `helper_sym_ternary_vec` pushes path constraints and concretizes its result.
    Returning an expression that represents the result of the comparison, instead of pushing path constraints,
    may give better results.
*/

/*
 * Symbolic equivalent of an SIMD ternary operation.
 *
 * This function performs the symbolic equivalent of computing an output vector of the form :
 * result[i] = arg1[i] <comparison_operator> arg2[i] ? arg1[i] : arg2[i]
 * where <vector>[i] denotes the ith element of the vector <vector>.
 *
 * For each element, the concrete result is used to determine if the ternary condition was true or false,
 * and path constraints are pushed accordingly.
 *
 * Args
 *      arg1_concrete, arg2_concrete : pointers to buffers that store the concrete values of the input vector operands
 *      arg1_symbolic, arg2_symbolic : symbolic expressions of the input vector operands
 *      comparison_operator : enum value that represents a comparison operator like <, >, ==, etc.
 *      concrete_result : pointer to a buffer that stores the concrete value of the result vector, as computed by the
 *          concrete execution of the SIMD operation
 *      vector_size : size of the vectors in bits
 *      vece : element size in bits = 8 * 2^vece
 * Returns
 *      An expression that corresponds to the symbolic result of the SIMD operation
 */
void *HELPER(sym_ternary_vec)(
        CPUArchState *env,
        void *arg1_concrete, void *arg1_symbolic,
        void *arg2_concrete, void *arg2_symbolic,
        /* TCGCond */ uint32_t comparison_operator, void *concrete_result,
        uint64_t vector_size, uint64_t vece
) {
    uint64_t element_size = vece_element_size(vece);

    g_assert(vector_size % element_size == 0);
    uint64_t element_count = vector_size / element_size;

    static void** arg1_elts      = NULL;
    static void** arg2_elts      = NULL;

    /* For each element, the condition of the ternary was true iff the element of the result is equal to the element
     * of arg1. */
    static int* concrete_condition_was_true = NULL;

    static uint64_t arg_len = 0;
    static uint64_t concrete_condition_len = 0;

    uint64_t arg_len_required = element_count * sizeof(void*);
    if (arg_len < arg_len_required) {
        arg1_elts = g_realloc(arg1_elts, arg_len_required);
        arg2_elts = g_realloc(arg2_elts, arg_len_required);

        arg_len = arg_len_required;
    }

    uint64_t concrete_condition_len_required = element_count * sizeof(int);
    if (concrete_condition_len < concrete_condition_len_required) {
        concrete_condition_was_true = g_realloc(concrete_condition_was_true, concrete_condition_len_required);

        concrete_condition_len = concrete_condition_len_required;
    }

    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    g_assert(element_size <= vector_size);

    if (arg1_symbolic == NULL && arg2_symbolic == NULL) {
        return NULL;
    }

    if (arg1_symbolic == NULL) {
        arg1_symbolic = _sym_build_integer_from_buffer(arg1_concrete, vector_size);
    }

    if (arg2_symbolic == NULL) {
        arg2_symbolic = _sym_build_integer_from_buffer(arg2_concrete, vector_size);
    }

    g_assert(_sym_bits_helper(arg1_symbolic) == _sym_bits_helper(arg2_symbolic) &&
             _sym_bits_helper(arg1_symbolic) == vector_size);

    split_expression(arg1_symbolic, element_count, element_size, arg1_elts);
    split_expression(arg2_symbolic, element_count, element_size, arg2_elts);

    for (int i = 0; i < element_count; i++) {
        void *result_element_ptr = element_address(concrete_result, i, element_size, vector_size);
        void *arg1_element_ptr = element_address(arg1_concrete, i, element_size, vector_size);
        concrete_condition_was_true[i] = memcmp(result_element_ptr, arg1_element_ptr, element_size / 8) == 0;
    }

    for (int i = 0; i < element_count; i++) {
        build_and_push_path_constraint(
                env,
                arg1_elts[i],
                arg2_elts[i],
                comparison_operator,
                concrete_condition_was_true[i]
        );
    }

    void *result_expression = concrete_condition_was_true[0] ? arg1_elts[0] : arg2_elts[0];
    for (int i = 1; i < element_count; i++) {
        result_expression = _sym_concat_helper(concrete_condition_was_true[i] ? arg1_elts[i] : arg2_elts[i],
                                               result_expression);
    }

    g_assert(_sym_bits_helper(result_expression) == vector_size);
    return result_expression;
}
