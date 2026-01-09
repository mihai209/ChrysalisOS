#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Chrysalis Script Interpreter Entry Point
 * Acts as /bin/cs
 * Prioritizes Disk (FAT32) if available.
 */
int cmd_cs_main(int argc, char** argv);

#ifdef __cplusplus
}
#endif