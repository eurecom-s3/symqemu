#include "qemu/osdep.h"
#include "tcg.h"
#include "exec/helper-proto.h"

#define SymExpr void*
#include "RuntimeCommon.h"

static void addition_test(void)
{
    void *result = helper_sym_add_i32(42, _sym_build_integer(42, 32),
                                      -42, NULL);
    g_assert_cmpint(_sym_bits_helper(result), ==, 32);
    g_assert_true(
        _sym_feasible(_sym_build_equal(result, _sym_build_integer(0, 32))));

    result = helper_sym_add_i64(42, _sym_build_integer(42, 64), -42, NULL);
    g_assert_cmpint(_sym_bits_helper(result), ==, 64);
    g_assert_true(
        _sym_feasible(_sym_build_equal(result, _sym_build_integer(0, 64))));
}

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    fclose(stdin);              /* for the Qsym backend */
    _sym_initialize();

    g_test_add_func("/sym/addition", addition_test);

    return g_test_run();
}
