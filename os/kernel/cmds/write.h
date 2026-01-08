#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* write <path> <text...>
 * Creates file or appends text with a newline.
 */
int cmd_write(int argc, char** argv);

#ifdef __cplusplus
}
#endif