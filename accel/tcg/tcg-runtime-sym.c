#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/qemu-print.h"

/* This should make it easy to spot bogus expressions in the debugger. */
#define NOT_IMPLEMENTED ((void*)0xAAAAAAAAAAAAAAAA)

void *HELPER(sym_add_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_add_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_sub_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_sub_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_and_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_and_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_or_i64)(uint64_t arg1, void *arg1_expr,
                         uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_or_i32)(uint32_t arg1, void *arg1_expr,
                         uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_xor_i64)(uint64_t arg1, void *arg1_expr,
                          uint64_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_xor_i32)(uint32_t arg1, void *arg1_expr,
                          uint32_t arg2, void *arg2_expr)
{
    /* TODO */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_load_i64)(uint64_t addr, void *addr_expr, uint64_t length)
{
    /* TODO */
    /* TODO try an alternative address */
    return NOT_IMPLEMENTED;
}

void *HELPER(sym_load_host_i64)(void *addr, uint64_t offset, uint64_t length)
{
    return HELPER(sym_load_i64)(
        (uint64_t)((uint8_t*)addr + offset), NULL, length);
}

void HELPER(sym_store_host_i64)(uint64_t value, void *value_expr,
                                void *addr, uint64_t offset, uint64_t length)
{
    /* TODO */
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
