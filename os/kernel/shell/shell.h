#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void shell_init();
void shell_handle_char(char c);

/* API pentru shortcuts */
void shell_reset_input(void);
void shell_prompt(void);

/* new: pollează inputul din buffer (apelat din loop-ul principal) */
void shell_poll_input(void);

/* Multi-instance support */
void shell_init_context(int id);
void shell_set_active_context(int id);

#ifdef __cplusplus
}
#endif
