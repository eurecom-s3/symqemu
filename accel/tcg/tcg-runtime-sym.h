DEF_HELPER_FLAGS_2(sym_add, TCG_CALL_NO_RWG, void, ptr, ptr)
DEF_HELPER_FLAGS_4(sym_sub_i32, TCG_CALL_NO_RWG_SE, ptr, i32, ptr, i32, ptr)
DEF_HELPER_FLAGS_4(sym_sub_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64, ptr)
