#include "mkdir.h"
#include "fat.h"
#include "../terminal.h"
#include "cd.h"
#include "../string.h"

extern "C" int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        terminal_writestring("Usage: mkdir <dirname>\n");
        return -1;
    }

    const char* arg = argv[1];
    char path[256];

    /* Handle absolute vs relative path */
    if (arg[0] == '/') {
        strncpy(path, arg, 255);
        path[255] = 0;
    } else {
        char cwd[256];
        cd_get_cwd(cwd, 256);
        
        strcpy(path, cwd);
        size_t len = strlen(path);
        if (len > 0 && path[len-1] != '/') {
            strcat(path, "/");
        }
        strcat(path, arg);
    }
    
    fat_automount();
    
    int res = fat32_create_directory(path);
    if (res == 0) {
        terminal_printf("Directory created: %s\n", path);
        return 0;
    } else {
        terminal_writestring("Error: Could not create directory.\n");
        return -1;
    }
}