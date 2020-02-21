#define SYM_HELPER_BINARY(name)                                                \
  DEF_HELPER_FLAGS_4(sym_##name##_i32, TCG_CALL_NO_RWG_SE, ptr,                \
                     i32, ptr, i32, ptr)                                       \
  DEF_HELPER_FLAGS_4(sym_##name##_i64, TCG_CALL_NO_RWG_SE, ptr,                \
                     i64, ptr, i64, ptr)

SYM_HELPER_BINARY(add)
SYM_HELPER_BINARY(sub)

SYM_HELPER_BINARY(shift_right)
SYM_HELPER_BINARY(arithmetic_shift_right)
SYM_HELPER_BINARY(shift_left)

SYM_HELPER_BINARY(and)
SYM_HELPER_BINARY(or)
SYM_HELPER_BINARY(xor)

#undef SYM_HELPER_BINARY

DEF_HELPER_FLAGS_3(sym_load_i64, TCG_CALL_NO_RWG_SE, ptr, i64, ptr, i64)
DEF_HELPER_FLAGS_3(sym_load_host_i64, TCG_CALL_NO_RWG_SE, ptr, ptr, i64, i64)
DEF_HELPER_FLAGS_5(sym_store_host_i64, TCG_CALL_NO_RWG, void, i64, ptr, ptr,
                   i64, i64)
