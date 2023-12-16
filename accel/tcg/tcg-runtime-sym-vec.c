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

void *HELPER(malloc)(uint64_t size){
    return malloc(size);
}

void HELPER(free)(void *ptr){
    free(ptr);
}

static void split_symbol(void* symbol, uint64_t element_count, uint64_t element_size, void* result[]){
    g_assert(_sym_bits_helper(symbol) % element_count == 0);
    for(uint64_t i = 0; i < element_count; i++){
        result[i] = _sym_extract_helper(symbol, (i + 1) * element_size - 1, i * element_size);
    }
}

static void *apply_op_and_merge(void* (*op)(void*, void*), void* arg1[], void* arg2[], uint64_t element_count){
    void* merged_result = op(arg1[0], arg2[0]);
    for(uint64_t i = 1; i < element_count; i++){
        merged_result = _sym_concat_helper(op(arg1[i], arg2[i]), merged_result);
    }
    return merged_result;
}

static uint64_t vece_element_size(uint64_t vece){
    /* see definition of vece here https://www.qemu.org/docs/master/devel/tcg-ops.html#host-vector-operations */
    return 8 << vece;
}

/*
 * Builds a symbolic expression for an SIMD operation on two vector operands.
 * Args
 *      arg1_concrete, arg2_concrete : concrete values of the input vector operands
 *      arg1_symbolic, arg2_symbolic : symbolic expressions of the input vector operands
 *      vector_size : size of the vectors in bits
 *      vece : element size in bits = 8 * 2^vece
 *      symbolic_operation : function for the symbolic operation applied on each element of the vectors
 * Returns
 *      A symbol that represents the output of the SIMD operation
 */
static void *build_symbol_for_vector_vector_op(
        void *arg1_concrete, void *arg1_symbolic,
        void *arg2_concrete, void *arg2_symbolic,
        uint64_t vector_size, uint64_t vece,
        void *(*symbolic_operation)(void *, void *)
) {
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
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

    uint64_t element_count = vector_size / element_size;
    void *arg1_elements[element_count];
    void *arg2_elements[element_count];
    split_symbol(arg1_symbolic, element_count, element_size, arg1_elements);
    split_symbol(arg2_symbolic, element_count, element_size, arg2_elements);

    void *result = apply_op_and_merge(symbolic_operation, arg1_elements, arg2_elements, element_count);
    g_assert(_sym_bits_helper(result) == vector_size);
    return result;
}

static void* build_symbol_for_vector_int32_op(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t vector_size, uint64_t vece, void* (*op)(void*, void*)){
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (arg1_expr == NULL && arg2_expr == NULL) {
        return NULL;
    }

    if (arg1_expr == NULL) {
        arg1_expr = _sym_build_integer_from_buffer(arg1, vector_size);
    }

    if (arg2_expr == NULL) {
        arg2_expr = _sym_build_integer(arg2, 32);
    }

    g_assert(_sym_bits_helper(arg1_expr) == vector_size);
    g_assert(_sym_bits_helper(arg2_expr) == 32);

    uint64_t element_count = vector_size / element_size;
    void* arg1_elements[element_count];
    void* arg2_elements[element_count];
    split_symbol(arg1_expr, element_count, element_size, arg1_elements);
    for(uint64_t i = 0; i < element_count; i++){
        arg2_elements[i] = arg2_expr;
    }

    void* result = apply_op_and_merge(op, arg1_elements, arg2_elements, element_count);
    g_assert(_sym_bits_helper(result) == vector_size);
    return result;
}

void *HELPER(sym_and_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_and);
}

void *HELPER(sym_or_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_or);
}

void *HELPER(sym_xor_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_xor);
}

void *HELPER(sym_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_add);
}

void *HELPER(sym_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_sub);
}

void *HELPER(sym_mul_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_mul);
}

void *HELPER(sym_signed_saturating_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_sadd_sat);
}

void *HELPER(sym_signed_saturating_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_ssub_sat);
}

void *HELPER(sym_unsigned_saturating_add_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_uadd_sat);
}

void *HELPER(sym_unsigned_saturating_sub_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_usub_sat);
}

void *HELPER(sym_shift_left_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_shift_left);
}

void *HELPER(sym_logical_shift_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_logical_shift_right);
}

void *HELPER(sym_arithmetic_shift_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece){
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_arithmetic_shift_right);
}

void *HELPER(sym_rotate_left_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_rotate_left);
}

void *HELPER(sym_rotate_right_vec)(void *arg1, void *arg1_expr, void *arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_vector_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_rotate_right);
}

void *HELPER(sym_shift_left_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_shift_left);
}

void *HELPER(sym_logical_shift_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_logical_shift_right);
}

void *HELPER(sym_arithmetic_shift_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_arithmetic_shift_right);
}

void *HELPER(sym_rotate_left_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_rotate_left);
}

void *HELPER(sym_rotate_right_vec_int32)(void *arg1, void *arg1_expr, uint32_t arg2, void *arg2_expr, uint64_t size, uint64_t vece) {
    return build_symbol_for_vector_int32_op(arg1, arg1_expr, arg2, arg2_expr, size, vece, _sym_build_rotate_right);
}

void *HELPER(sym_duplicate_value_into_vec)(void *value_expr, uint64_t vector_size, uint64_t vece){
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (value_expr == NULL) {
        return NULL;
    }

    g_assert(_sym_bits_helper(value_expr) == 32 || _sym_bits_helper(value_expr) == 64);

    void *resized_value_expr;

    if (_sym_bits_helper(value_expr) == element_size) {
        resized_value_expr = value_expr;
    } else if (_sym_bits_helper(value_expr) < element_size) {
        resized_value_expr = _sym_build_zext(value_expr, element_size);
    } else {
        resized_value_expr = _sym_extract_helper(value_expr, element_size - 1, 0);
    }

    uint64_t element_count = vector_size / element_size;
    void *result_symbol = resized_value_expr;
    for(uint64_t i = 1; i < element_count; i++){
        result_symbol = _sym_concat_helper(resized_value_expr, result_symbol);
    }
    g_assert(_sym_bits_helper(result_symbol) == vector_size);
    return result_symbol;
}

void *HELPER(sym_load_and_duplicate_into_vec)(void *addr, uint64_t offset, uint64_t vector_size, uint64_t vece){
    g_assert(vector_size == 64 || vector_size == 128 || vector_size == 256);
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    void *value_expr = helper_sym_load_host_v(addr, offset, element_size / 8);
    g_assert(value_expr == NULL || _sym_bits_helper(value_expr) == element_size);

    return helper_sym_duplicate_value_into_vec(value_expr, vector_size, vece);
}

static void *element_address(void *concrete_vector, uint64_t element_index, uint64_t element_size, uint64_t vector_size){
    void* result = concrete_vector + element_index * element_size / 8;
    g_assert(result + element_size <= concrete_vector + vector_size);
    return result;
}

void HELPER(sym_cmp_vec)(
        CPUArchState *env,
        void *arg1, void *arg1_expr,
        void *arg2, void *arg2_expr,
        uint32_t comparison_operator, void *concrete_result,
        uint64_t vector_size, uint64_t vece
) {
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (arg1_expr == NULL && arg2_expr == NULL) {
        return;
    }

    if (arg1_expr == NULL) {
        arg1_expr = _sym_build_integer_from_buffer(arg1, vector_size);
    }

    if (arg2_expr == NULL) {
        arg2_expr = _sym_build_integer_from_buffer(arg2, vector_size);
    }

    g_assert(_sym_bits_helper(arg1_expr) == _sym_bits_helper(arg2_expr) && _sym_bits_helper(arg1_expr) == vector_size);

    uint64_t element_count = vector_size / element_size;
    void *arg1_elements[element_count];
    void *arg2_elements[element_count];
    split_symbol(arg1_expr, element_count, element_size, arg1_elements);
    split_symbol(arg2_expr, element_count, element_size, arg2_elements);

    for (uint64_t i = 0; i < element_count; i++) {
        uint8_t is_taken = * (uint8_t*) element_address(concrete_result, i, element_size, vector_size);
        build_and_push_path_constraint(
                env,
                arg1_elements[i],
                arg2_elements[i],
                comparison_operator,
                is_taken
        );
    }
}

void *HELPER(sym_minmax_vec)(
        CPUArchState *env,
        void *arg1, void *arg1_expr,
        void *arg2, void *arg2_expr,
        uint32_t comparison_operator, void *concrete_result,
        uint64_t vector_size, uint64_t vece
){
    uint64_t element_size = vece_element_size(vece);
    g_assert(element_size <= vector_size);
    g_assert(vector_size % element_size == 0);

    if (arg1_expr == NULL && arg2_expr == NULL) {
        return NULL;
    }

    if (arg1_expr == NULL) {
        arg1_expr = _sym_build_integer_from_buffer(arg1, vector_size);
    }

    if (arg2_expr == NULL) {
        arg2_expr = _sym_build_integer_from_buffer(arg2, vector_size);
    }

    g_assert(_sym_bits_helper(arg1_expr) == _sym_bits_helper(arg2_expr) && _sym_bits_helper(arg1_expr) == vector_size);

    uint64_t element_count = vector_size / element_size;
    void *arg1_elements[element_count];
    void *arg2_elements[element_count];
    int is_taken[element_count];
    split_symbol(arg1_expr, element_count, element_size, arg1_elements);
    split_symbol(arg2_expr, element_count, element_size, arg2_elements);
    for (int i = 0; i < element_count; i++) {
        void *result_element_ptr = element_address(concrete_result, i, element_size, vector_size);
        void *arg1_element_ptr = element_address(arg1, i, element_size, vector_size);
        is_taken[i] = memcmp(result_element_ptr, arg1_element_ptr, element_size / 8) == 0;
    }

    for (int i = 0; i < element_count; i++) {
        build_and_push_path_constraint(
                env,
                arg1_elements[i],
                arg2_elements[i],
                comparison_operator,
                is_taken[i]
        );
    }

    void *result_symbol = is_taken[0] ? arg1_elements[0] : arg2_elements[0];
    for (int i = 1; i < element_count; i++) {
        result_symbol = _sym_concat_helper(is_taken[i] ? arg1_elements[i] : arg2_elements[i], result_symbol);
    }

    g_assert(_sym_bits_helper(result_symbol) == vector_size);
    return result_symbol;
}