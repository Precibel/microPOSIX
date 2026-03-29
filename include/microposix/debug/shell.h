#ifndef MICROPOSIX_SHELL_H
#define MICROPOSIX_SHELL_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the Resident OS Shell
void mp_shell_init(void);

// Start the shell task (low priority)
void mp_shell_start(void);

// Command structure
typedef void (*mp_shell_cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *cmd;
    const char *help;
    mp_shell_cmd_func_t func;
} mp_shell_cmd_t;

// Register a new shell command (can be used by OS modules)
void mp_shell_register_cmd(const mp_shell_cmd_t *cmd);

#endif // MICROPOSIX_SHELL_H
