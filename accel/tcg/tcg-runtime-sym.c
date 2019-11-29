#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/qemu-print.h"

void HELPER(sym_add)(void *arg1, void *arg2)
{
    qemu_printf("Now adding expressions %p and %p\n", arg1, arg2);
}

uint64_t HELPER(sym_sub_i64)(uint64_t arg1, uint64_t arg1_expr,
                             uint64_t arg2, uint64_t arg2_expr)
{
    // TODO
    return 0xAAAAAAAAAAAAAAAA;
}

uint64_t HELPER(sym_sub_i32)(uint32_t arg1, uint64_t arg1_expr,
                             uint32_t arg2, uint64_t arg2_expr)
{
    // TODO
    return 0xAAAAAAAAAAAAAAAA;
}
