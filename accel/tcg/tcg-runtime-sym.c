#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/qemu-print.h"

/* This should make it easy to spot bogus expressions in the debugger. */
#define NOT_IMPLEMENTED ((void*)0xAAAAAAAAAAAAAAAA)

void HELPER(sym_add)(void *arg1, void *arg2)
{
    qemu_printf("Now adding expressions %p and %p\n", arg1, arg2);
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
