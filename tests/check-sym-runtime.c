#include "qemu/osdep.h"
#include "tcg.h"
#include "exec/helper-proto.h"

#define SymExpr void*
#include "RuntimeCommon.h"

#define assert_equal(expr, value, bits)                                        \
    g_assert_true(_sym_feasible(                                               \
        _sym_build_equal(expr, _sym_build_integer(value, bits))))

#define TEST(handler, x, y, res)                                               \
    static void handler##_test(void) {                                         \
        void *result32 =                                                       \
            helper_sym_##handler##_i32(x, _sym_build_integer(x, 32), y, NULL); \
        g_assert_cmpint(_sym_bits_helper(result32), ==, 32);                   \
        assert_equal(result32, res, 32);                                       \
                                                                               \
        void *result64 =                                                       \
            helper_sym_##handler##_i64(x, _sym_build_integer(x, 64), y, NULL); \
        g_assert_cmpint(_sym_bits_helper(result64), ==, 64);                   \
        assert_equal(result64, res, 64);                                       \
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
TEST(andc, 0xAA, 0x55, 0xAA);
TEST(eqv, 0xAA, 0xAF, -(0xFF - 0xFA + 1));
TEST(nand, 0xAA, 0xA5, -(0xFF - 0x5F + 1));
TEST(nor, 0xAA, 0x55, -0x100);
TEST(orc, 0xAA, 0xAA, -1);

#undef TEST

static void not_test(void)
{
    assert_equal(helper_sym_not(_sym_build_integer(0x55FF00FF, 32)),
                 0xAA00FF00, 32);
    assert_equal(helper_sym_not(_sym_build_integer(0x55FF00FF, 64)),
                 0xFFFFFFFFAA00FF00, 64);
}

static void neg_test(void)
{
    assert_equal(helper_sym_neg(_sym_build_integer(5, 32)),
                 -5, 32);
    assert_equal(helper_sym_neg(_sym_build_integer(5, 64)),
                 -5, 64);
}

static void sext_test(void)
{
    /* 32-bit inputs */
    assert_equal(helper_sym_sext(_sym_build_integer(0x55, 32), 1),
                 0x55, 32);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0, 32), 1),
                 0xfffffff0, 32);
    assert_equal(helper_sym_sext(_sym_build_integer(0x55, 32), 2),
                 0x55, 32);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0, 32), 2),
                 0xf0, 32);
    assert_equal(helper_sym_sext(_sym_build_integer(0x5555, 32), 2),
                 0x5555, 32);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0f0, 32), 2),
                 0xfffff0f0, 32);

    /* 64-bit inputs */
    assert_equal(helper_sym_sext(_sym_build_integer(0x55, 64), 1),
                 0x55, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0, 64), 1),
                 0xfffffffffffffff0, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0x55, 64), 2),
                 0x55, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0, 64), 2),
                 0xf0, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0x5555, 64), 2),
                 0x5555, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0f0, 64), 2),
                 0xfffffffffffff0f0, 64);
    assert_equal(helper_sym_sext(_sym_build_integer(0xf0f0f0f0, 64), 4),
                 0xfffffffff0f0f0f0, 64);
}

static void zext_test(void)
{
    /* 32-bit inputs */
    assert_equal(helper_sym_zext(_sym_build_integer(0x55, 32), 1),
                 0x55, 32);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0, 32), 1),
                 0xf0, 32);
    assert_equal(helper_sym_zext(_sym_build_integer(0x55, 32), 2),
                 0x55, 32);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0, 32), 2),
                 0xf0, 32);
    assert_equal(helper_sym_zext(_sym_build_integer(0x5555, 32), 2),
                 0x5555, 32);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0f0, 32), 2),
                 0xf0f0, 32);

    /* 64-bit inputs */
    assert_equal(helper_sym_zext(_sym_build_integer(0x55, 64), 1),
                 0x55, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0, 64), 1),
                 0xf0, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0x55, 64), 2),
                 0x55, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0, 64), 2),
                 0xf0, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0x5555, 64), 2),
                 0x5555, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0f0, 64), 2),
                 0xf0f0, 64);
    assert_equal(helper_sym_zext(_sym_build_integer(0xf0f0f0f0, 64), 4),
                 0xf0f0f0f0, 64);
}

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
    REGISTER_TEST(andc);
    REGISTER_TEST(eqv);
    REGISTER_TEST(nand);
    REGISTER_TEST(nor);
    REGISTER_TEST(orc);
    REGISTER_TEST(not);
    REGISTER_TEST(neg);
    REGISTER_TEST(sext);
    REGISTER_TEST(zext);

#undef REGISTER_TEST

    return g_test_run();
}
