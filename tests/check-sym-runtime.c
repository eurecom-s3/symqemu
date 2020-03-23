#include "qemu/osdep.h"
#include "tcg.h"
#include "exec/helper-proto.h"

#define SymExpr void*
#include "RuntimeCommon.h"

#define TEST(handler, x, y, res)                                               \
    static void handler##_test(void) {                                         \
        void *result32 =                                                       \
            helper_sym_##handler##_i32(x, _sym_build_integer(x, 32), y, NULL); \
        g_assert_cmpint(_sym_bits_helper(result32), ==, 32);                   \
        g_assert_true(_sym_feasible(                                           \
            _sym_build_equal(result32, _sym_build_integer(res, 32))));         \
                                                                               \
        void *result64 =                                                       \
            helper_sym_##handler##_i64(x, _sym_build_integer(x, 64), y, NULL); \
        g_assert_cmpint(_sym_bits_helper(result64), ==, 64);                   \
        g_assert_true(_sym_feasible(                                           \
            _sym_build_equal(result64, _sym_build_integer(res, 64))));         \
    }

TEST(add, 42, -42, 0);
TEST(sub, 1234, 4321, -3087);
TEST(mul, 5, 6, 30);
TEST(div, 144, -24, -6);
TEST(divu, 144, 24, 6);
TEST(rem, 101, -5, 1);
TEST(remu, 101, 5, 1);
TEST(and, 0xAA, 0x55, 0x00);
TEST(or, 0xAA, 0x55, 0xFF);
TEST(xor, 0xAA, 0xA5, 0x0F);
TEST(shift_right, 0xFF00, 3, 0x1FE0);
TEST(arithmetic_shift_right, -100, 2, -25);
TEST(shift_left, 22, 1, 44);

#undef TEST

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    fclose(stdin);              /* for the Qsym backend */
    _sym_initialize();

#define REGISTER_TEST(name) g_test_add_func("/sym/" #name, name##_test)

    REGISTER_TEST(add);
    REGISTER_TEST(sub);
    REGISTER_TEST(mul);
    REGISTER_TEST(div);
    REGISTER_TEST(divu);
    REGISTER_TEST(rem);
    REGISTER_TEST(remu);
    REGISTER_TEST(and);
    REGISTER_TEST(or);
    REGISTER_TEST(xor);
    REGISTER_TEST(shift_right);
    REGISTER_TEST(arithmetic_shift_right);
    REGISTER_TEST(shift_left);

#undef REGISTER_TEST

    return g_test_run();
}
