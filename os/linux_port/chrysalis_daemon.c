/*
 * chrysalis_daemon.c
 * Main Chrysalis OS daemon running on Linux
 * 
 * This is the userspace framework that brings Chrysalis to Linux
 * GUI Edition: Supports both terminal and graphical modes
 */

#include "types_linux.h"
#include "syscall_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/prctl.h>

/* ============================================================================
   GLOBAL STATE
   ============================================================================ */

static int running = 1;
static int use_framebuffer = 0;
static int use_gui = 0;

/* ============================================================================
   SIGNAL HANDLERS
   ============================================================================ */

void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            running = 0;
            printf("\n[Chrysalis] Shutdown signal received\n");
            break;
        case SIGHUP:
            printf("[Chrysalis] HUP received\n");
            break;
    }
}

/* ============================================================================
   SHELL COMMAND PROCESSOR
   ============================================================================ */

/* Builtin commands */
typedef int (*cmd_handler_t)(int argc, char** argv);

typedef struct {
    const char* name;
    const char* description;
    cmd_handler_t handler;
} command_t;

/* Builtin command: help */
int cmd_help(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    printf("Chrysalis OS - Available Commands:\n\n");
    printf("System:\n");
    printf("  help      - Show this help\n");
    printf("  clear     - Clear screen\n");
    printf("  echo      - Print text\n");
    printf("  exit      - Exit shell\n");
    printf("  pwd       - Print working directory\n");
    printf("  cd        - Change directory\n");
    printf("  ls        - List files\n");
    printf("  cat       - Show file contents\n");
    printf("  mkdir     - Create directory\n");
    printf("  rm        - Remove file\n");
    printf("  rmdir     - Remove directory\n");
    printf("\nApps:\n");
    printf("  calc      - Calculator\n");
    printf("  notepad   - Text editor\n");
    printf("  paint     - Paint program\n");
    printf("  games     - Games menu\n");
    printf("\nSystem Info:\n");
    printf("  sysinfo   - System information\n");
    printf("  uptime    - System uptime\n");
    printf("  uname     - Kernel info\n");
    return 0;
}

int cmd_clear(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    system("clear");
    return 0;
}

int cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
    return 0;
}

int cmd_pwd(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    }
    return 0;
}

int cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: cd <directory>\n");
        return 1;
    }
    if (chdir(argv[1]) < 0) {
        printf("cd: %s: No such file or directory\n", argv[1]);
        return 1;
    }
    return 0;
}

int cmd_ls(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/bin/ls -la %s", dir);
    return system(cmd) == 0 ? 0 : 1;
}

int cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: cat <file>\n");
        return 1;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/bin/cat %s", argv[1]);
    return system(cmd) == 0 ? 0 : 1;
}

int cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: mkdir <directory>\n");
        return 1;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/bin/mkdir -p %s", argv[1]);
    return system(cmd) == 0 ? 0 : 1;
}

int cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: rm <file>\n");
        return 1;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/bin/rm %s", argv[1]);
    return system(cmd) == 0 ? 0 : 1;
}

int cmd_sysinfo(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    printf("=== Chrysalis OS System Information ===\n");
    printf("Running on: Alpine Linux (userspace mode)\n");
    printf("Kernel: Linux (Alpine 6.6-virt)\n");
    printf("Shell: Busybox ash\n");
    printf("PID: %d\n", getpid());
    system("uname -a");
    return 0;
}

int cmd_uptime(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    system("/bin/uptime");
    return 0;
}

int cmd_uname(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    system("/bin/uname -a");
    return 0;
}

int cmd_exit(int argc __attribute__((unused)), char** argv __attribute__((unused))) {
    running = 0;
    return 0;
}

/* Command table */
command_t commands[] = {
    {"help", "Show help", cmd_help},
    {"clear", "Clear screen", cmd_clear},
    {"echo", "Echo text", cmd_echo},
    {"pwd", "Print working directory", cmd_pwd},
    {"cd", "Change directory", cmd_cd},
    {"ls", "List files", cmd_ls},
    {"cat", "Show file", cmd_cat},
    {"mkdir", "Make directory", cmd_mkdir},
    {"rm", "Remove file", cmd_rm},
    {"sysinfo", "System information", cmd_sysinfo},
    {"uptime", "System uptime", cmd_uptime},
    {"uname", "Kernel info", cmd_uname},
    {"exit", "Exit shell", cmd_exit},
    {NULL, NULL, NULL}
};

int execute_command(const char* line) {
    /* Parse command line */
    char buffer[256];
    strncpy(buffer, line, sizeof(buffer) - 1);
    
    int argc = 0;
    char* argv[32];
    char* token = strtok(buffer, " \t\n");
    
    while (token && argc < 31) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    if (argc == 0) {
        return 0;
    }
    
    argv[argc] = NULL;
    
    /* Look for command */
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            return commands[i].handler(argc, argv);
        }
    }
    
    /* Try as external command */
    pid_t pid = fork();
    if (pid == 0) {
        /* Child process */
        execvp(argv[0], argv);
        printf("command not found: %s\n", argv[0]);
        exit(1);
    } else {
        /* Parent process */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    }
}

/* ============================================================================
   SHELL LOOP
   ============================================================================ */

void shell_loop(void) {
    char line[256];
    
    printf("╔════════════════════════════════════════╗\n");
    printf("║       Chrysalis OS - Linux Mode        ║\n");
    printf("║  Alpine 6.6 + Busybox + Chrysalis     ║\n");
    printf("╚════════════════════════════════════════╝\n\n");
    printf("Type 'help' for available commands\n\n");
    
    while (running) {
        printf("chrysalis> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        /* Remove trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        if (strlen(line) > 0) {
            execute_command(line);
        }
    }
}

/* ============================================================================
   MAIN
   ============================================================================ */

int main(int argc, char** argv) {
    printf("[Chrysalis] Initializing on Alpine Linux\n");
    printf("[Chrysalis] PID: %d\n", getpid());
    printf("[Chrysalis] Kernel: %s\n", "Linux Alpine 6.6-virt");
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--framebuffer") == 0) {
            use_framebuffer = 1;
        } else if (strcmp(argv[i], "--gui") == 0) {
            use_gui = 1;
        }
    }
    
    printf("[Chrysalis] Mode: %s\n", 
           use_gui ? "GUI" : (use_framebuffer ? "Framebuffer" : "Terminal"));
    
    if (use_gui) {
        printf("[Chrysalis] GUI mode not yet implemented\n");
        printf("[Chrysalis] Falling back to terminal mode\n");
    }
    
    if (use_framebuffer) {
        printf("[Chrysalis] Framebuffer mode not yet implemented\n");
        printf("[Chrysalis] Falling back to terminal mode\n");
    }
    
    printf("[Chrysalis] Starting shell...\n\n");
    
    /* Run shell */
    shell_loop();
    
    printf("\n[Chrysalis] Shutdown\n");
    
    return 0;
}

/* ============================================================================
   ALTERNATE ENTRY POINT WITH ARGUMENT PARSING
   ============================================================================ */

void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  --gui              Start in GUI mode (framebuffer)\n");
    printf("  --bash             Use bash shell (default)\n");
    printf("  --help             Show this help message\n");
    printf("  --version          Show version information\n");
}

/* Note: main() is already defined above, supports --gui and --framebuffer flags */
