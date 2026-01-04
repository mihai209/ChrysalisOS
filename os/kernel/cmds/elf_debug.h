#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "../elf/elf.h"

void elf_debug_print_info(const elf_load_info_t* info);
int cmd_elf_debug(int argc, char** argv); /* optional command entry */

#ifdef __cplusplus
}
#endif
