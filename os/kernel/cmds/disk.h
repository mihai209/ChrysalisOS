#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Partition assign table (volatile, only in-memory) */
struct part_assign { int used; char letter; uint32_t lba; uint32_t count; };
extern struct part_assign g_assigns[26];

/* Shell command entrypoint. Adapt this to your shell command registration.
* Example: register_command("disk", cmd_disk);
* The shell should call cmd_disk(arg_string) where arg_string is the rest
* of the user input (may be NULL or empty).
*/
void cmd_disk(const char* args);


#ifdef __cplusplus
}
#endif