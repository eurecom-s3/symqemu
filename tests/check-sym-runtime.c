/*
 * This file is part of SymQEMU.
 *
 * SymQEMU is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * SymQEMU is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SymQEMU. If not, see <https://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "tcg.h"
#include "exec/helper-proto.h"

#define SymExpr void*
#include "RuntimeCommon.h"

/* This is a hack to make guest-to-host address translation think that guest and
 * host addresses are the same (see tlb_vaddr_to_host in the CONFIG_USER_ONLY
 * case). */
unsigned long guest_base = 0;

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

static void sext_i32_i64_test(void)
{
    assert_equal(helper_sym_sext_i32_i64(_sym_build_integer(0x55555555, 32)),
                 0x55555555, 64);
    assert_equal(helper_sym_sext_i32_i64(_sym_build_integer(0xa5555555, 32)),
                 0xffffffffa5555555, 64);
}

static void zext_i32_i64_test(void)
{
    assert_equal(helper_sym_zext_i32_i64(_sym_build_integer(0x55555555, 32)),
                 0x55555555, 64);
    assert_equal(helper_sym_zext_i32_i64(_sym_build_integer(0xa5555555, 32)),
                 0xa5555555, 64);
}

static void bswap_test(void)
{
    /* 32-bit input */
    assert_equal(helper_sym_bswap(_sym_build_integer(0xAABB, 32), 2),
                 0xBBAA, 32);
    assert_equal(helper_sym_bswap(_sym_build_integer(0xAABBCCDD, 32), 4),
                 0xDDCCBBAA, 32);

    /* 64-bit input */
    assert_equal(helper_sym_bswap(_sym_build_integer(0xAABB, 64), 2),
                 0xBBAA, 64);
    assert_equal(helper_sym_bswap(_sym_build_integer(0xAABBCCDD, 64), 4),
                 0xDDCCBBAA, 64);
    assert_equal(helper_sym_bswap(_sym_build_integer(0xAABBCCDD00112233, 64), 8),
                 0x33221100DDCCBBAA, 64);
}

static void rotate_left_test(void)
{
    assert_equal(helper_sym_rotate_left_i32(
                     0xAABBCCDD, _sym_build_integer(0xAABBCCDD, 32),
                     4, _sym_build_integer(4, 32)),
                 0xABBCCDDA, 32);
    assert_equal(helper_sym_rotate_left_i64(
                     0xAABBCCDD00112233, NULL,
                     4, _sym_build_integer(4, 64)),
                 0xABBCCDD00112233A, 64);
}

static void rotate_right_test(void)
{
    assert_equal(helper_sym_rotate_right_i32(
                     0xAABBCCDD, _sym_build_integer(0xAABBCCDD, 32),
                     4, _sym_build_integer(4, 32)),
                 0xDAABBCCD, 32);
    assert_equal(helper_sym_rotate_right_i64(
                     0xAABBCCDD00112233, NULL,
                     4, _sym_build_integer(4, 64)),
                 0x3AABBCCDD0011223, 64);
}

static void extract_test(void)
{
    assert_equal(helper_sym_extract_i32(
                     _sym_build_integer(0xAABBCCDD, 32), 4, 8),
                 0xCD, 32);
    assert_equal(helper_sym_extract_i64(
                     _sym_build_integer(0xAABBCCDD00112233, 64), 24, 28),
                 0xBCCDD00, 64);
}

static void sextract_test(void)
{
    assert_equal(helper_sym_sextract_i32(
                     _sym_build_integer(0xAABBCCDD, 32), 4, 8),
                 0xFFFFFFCD, 32);
    assert_equal(helper_sym_sextract_i64(
                     _sym_build_integer(0xAABBCCDD00112233, 64), 24, 31),
                 0x3BCCDD00, 64);
}

static void extract2_test(void)
{
    assert_equal(helper_sym_extract2_i32(
                     0xAABBCCDD, _sym_build_integer(0xAABBCCDD, 32),
                     0x00112233, NULL,
                     16),
                 0xCCDD0011, 32);
    assert_equal(helper_sym_extract2_i64(
                     0xAABBCCDD00112233, _sym_build_integer(0xAABBCCDD00112233, 64),
                     0x445566778899ABCD, NULL,
                     12),
                 0x233445566778899A, 64);
}

static void deposit_test(void)
{
    assert_equal(helper_sym_deposit_i32(
                     0xAAB00CDD, _sym_build_integer(0xAAB00CDD, 32),
                     0xBC, NULL,
                     12, 8),
                 0xAABBCCDD, 32);
    assert_equal(helper_sym_deposit_i64(
                     0xAABBCCDD00112233, _sym_build_integer(0xAABBCCDD00112233, 64),
                     0xEEFF, NULL,
                     16, 16),
                 0xAABBCCDDEEFF2233, 64);
}

static void load_store_guest_test(void)
{
    uint8_t memory[32];
    CPUArchState dummy_state;

    memset(memory, 0xAA, 16);
    memset(&memory[16], 0xBB, 16);

    /* Loading concrete memory should give us a null expression. */
    g_assert_true(helper_sym_load_guest_i64(
                      &dummy_state, (target_ulong)&memory[16], NULL, 8, 0)
                  == NULL);

    /* Store some symbolic data in the middle of the buffer. */
    *((uint32_t*)&memory[16]) = 0x11223344;
    helper_sym_store_guest_i64(
        &dummy_state,
        0x11223344, _sym_build_integer(0x11223344, 64),
        (target_ulong)&memory[16], NULL,
        4, 0);

    *((uint16_t*)&memory[20]) = 0x5566;
    helper_sym_store_guest_i32(
        &dummy_state,
        0x5566, _sym_build_integer(0x5566, 32),
        (target_ulong)&memory[20], NULL,
        2, 0);

    /* Buffer: [AA...AA 44 33 22 11 66 55 BB...BB] */

    /* Make sure that only the four bytes are symbolic, and that reading partly
     * symbolic data takes the concrete data into account. */
    assert_equal(helper_sym_load_guest_i64(
                     &dummy_state, (target_ulong)&memory[16], NULL, 1, 0),
                 0x44, 64);
    assert_equal(helper_sym_load_guest_i64(
                     &dummy_state, (target_ulong)&memory[16], NULL, 2, 0),
                 0x3344, 64);
    assert_equal(helper_sym_load_guest_i64(
                     &dummy_state, (target_ulong)&memory[16], NULL, 4, 0),
                 0x11223344, 64);
    assert_equal(helper_sym_load_guest_i64(
                     &dummy_state, (target_ulong)&memory[16], NULL, 8, 0),
                 0xBBBB556611223344, 64);

    assert_equal(helper_sym_load_guest_i32(
                     &dummy_state, (target_ulong)&memory[16], NULL, 1, 0),
                 0x44, 32);
    assert_equal(helper_sym_load_guest_i32(
                     &dummy_state, (target_ulong)&memory[16], NULL, 2, 0),
                 0x3344, 32);
    assert_equal(helper_sym_load_guest_i32(
                     &dummy_state, (target_ulong)&memory[16], NULL, 4, 0),
                 0x11223344, 32);

    assert_equal(helper_sym_load_guest_i32(
                     &dummy_state, (target_ulong)&memory[14], NULL, 4, 0),
                 0x3344AAAA, 32);

    /* Make the backend forget about the symbolic data (just in case). */
    _sym_write_memory(memory, sizeof(memory), NULL, false);
}

static void load_store_host_test(void)
{
    uint8_t memory[32];

    memset(memory, 0xAA, 16);
    memset(&memory[16], 0xBB, 16);

    /* Loading concrete memory should give us a null expression. */
    g_assert_true(helper_sym_load_host_i64(memory, 16, 8) == NULL);

    /* Store some symbolic data in the middle of the buffer. */
    *((uint32_t*)&memory[16]) = 0x11223344;
    helper_sym_store_host_i64(0x11223344, _sym_build_integer(0x11223344, 64),
                              memory, 16, 4);

    *((uint16_t*)&memory[20]) = 0x5566;
    helper_sym_store_host_i32(0x5566, _sym_build_integer(0x5566, 32),
                              memory, 20, 2);

    /* Buffer: [AA...AA 44 33 22 11 66 55 BB...BB] */

    /* Make sure that only the four bytes are symbolic, and that reading partly
     * symbolic data takes the concrete data into account. */
    assert_equal(helper_sym_load_host_i64(memory, 16, 1), 0x44, 64);
    assert_equal(helper_sym_load_host_i64(memory, 16, 2), 0x3344, 64);
    assert_equal(helper_sym_load_host_i64(memory, 16, 4), 0x11223344, 64);
    assert_equal(helper_sym_load_host_i64(memory, 16, 8), 0xBBBB556611223344, 64);

    assert_equal(helper_sym_load_host_i32(memory, 16, 1), 0x44, 32);
    assert_equal(helper_sym_load_host_i32(memory, 16, 2), 0x3344, 32);
    assert_equal(helper_sym_load_host_i32(memory, 16, 4), 0x11223344, 32);

    assert_equal(helper_sym_load_host_i32(memory, 14, 4), 0x3344AAAA, 32);

    /* Make the backend forget about the symbolic data (just in case). */
    _sym_write_memory(memory, sizeof(memory), NULL, false);
}

static void muluh_test(void)
{
    void *result = helper_sym_muluh_i64(
        0xAABBCCDDEEFF0011, _sym_build_integer(0xAABBCCDDEEFF0011, 64),
        0x0000000100000000, NULL);

    assert_equal(result, 0xAABBCCDD, 64);
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
    REGISTER_TEST(sext_i32_i64);
    REGISTER_TEST(zext_i32_i64);
    REGISTER_TEST(bswap);
    REGISTER_TEST(rotate_left);
    REGISTER_TEST(rotate_right);
    REGISTER_TEST(extract);
    REGISTER_TEST(sextract);
    REGISTER_TEST(extract2);
    REGISTER_TEST(deposit);
    REGISTER_TEST(load_store_guest);
    REGISTER_TEST(load_store_host);
    REGISTER_TEST(muluh);
#undef REGISTER_TEST

    return g_test_run();
}
