void *build_and_push_path_constraint(CPUArchState *env, void *arg1_expr, void *arg2_expr, uint32_t comparison_operator, uint8_t is_taken);
target_ulong get_pc(CPUArchState *env);
void *sym_rotate_left(void *arg1_expr, void *arg2_expr);
void *sym_rotate_right(void *arg1_expr, void *arg2_expr);
