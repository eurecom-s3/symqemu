DEF_HELPER_FLAGS_4(sym_add_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_add_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)
DEF_HELPER_FLAGS_4(sym_sub_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_sub_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)

DEF_HELPER_FLAGS_4(sym_shift_right_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_shift_right_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)
DEF_HELPER_FLAGS_4(sym_arithmetic_shift_right_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_arithmetic_shift_right_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)
DEF_HELPER_FLAGS_4(sym_shift_left_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_shift_left_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)

DEF_HELPER_FLAGS_2(sym_load_i64, TCG_CALL_NO_RWG_SE, ptr, i64, i64)
