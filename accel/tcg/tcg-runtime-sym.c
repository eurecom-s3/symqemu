#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "qemu/qemu-print.h"

void HELPER(sym_add)(void *arg1, void *arg2)
{
    qemu_printf("Now adding expressions %p and %p\n", arg1, arg2);
}
