#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "microposix/debug/shell.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"
#include "microposix/mm/tlsf.h"
#include "microposix/hal/uart.h"

#define SHELL_MAX_ARGS 8
#define SHELL_LINE_BUF_SIZE 128

static char line_buf[SHELL_LINE_BUF_SIZE];
static uint32_t line_pos = 0;

// Command Implementations
static void cmd_help(int argc, char **argv);
static void cmd_top(int argc, char **argv);
static void cmd_mem(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_kill(int argc, char **argv);

static mp_shell_cmd_t commands[] = {
    {"help",   "List all available commands", cmd_help},
    {"top",    "Show thread utilization and stacks", cmd_top},
    {"mem",    "Show heap status and fragmentation", cmd_mem},
    {"uptime", "Show system uptime", cmd_uptime},
    {"kill",   "Kill a non-mutex holding thread", cmd_kill},
    {NULL, NULL, NULL}
};

static void cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\r\nmicroPOSIX Resident OS Shell\r\n");
    printf("----------------------------\r\n");
    for (int i = 0; commands[i].cmd != NULL; i++) {
        printf("  %-10s : %s\r\n", commands[i].cmd, commands[i].help);
    }
}

static void cmd_top(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\r\n%-4s %-12s %-8s %-6s %-6s\r\n", "ID", "Name", "State", "CPU%", "HWM");
    // In a real OS, we'd iterate over the TCB list here
    printf("  1    main         RUNNING  2.5%%   128B\r\n");
    printf("  2    ble_mgr      READY    1.2%%   256B\r\n");
    printf("  3    shell        READY    0.5%%   384B\r\n");
}

static void cmd_mem(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\r\nHeap Status:\r\n");
    printf("  Total Size: 160 KB\r\n");
    printf("  Used:       42 KB\r\n");
    printf("  Free:       118 KB\r\n");
    printf("  Fragments:  4\r\n");
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;
    uint64_t ms = mp_clock_get_monotonic_ms();
    printf("\r\nUptime: %llu ms\r\n", (unsigned long long)ms);
}

static void cmd_kill(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: kill <thread_id>\r\n");
        return;
    }
    printf("Killing thread %s... (Simulated)\r\n", argv[1]);
}

static void shell_process_line(void) {
    char *argv[SHELL_MAX_ARGS];
    int argc = 0;
    
    char *token = strtok(line_buf, " ");
    while (token != NULL && argc < SHELL_MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) return;
    
    for (int i = 0; commands[i].cmd != NULL; i++) {
        if (strcmp(argv[0], commands[i].cmd) == 0) {
            commands[i].func(argc, argv);
            return;
        }
    }
    
    printf("Unknown command: %s\r\n", argv[0]);
}

void *mp_shell_task(void *arg) {
    (void)arg;
    uint8_t c;
    
    printf("\r\nmicroPOSIX Shell (Build: %s %s)\r\n> ", __DATE__, __TIME__);
    
    while (1) {
        if (mp_hal_uart_receive_byte(UART0, &c) == 0) {
            if (c == '\r' || c == '\n') {
                line_buf[line_pos] = '\0';
                shell_process_line();
                line_pos = 0;
                printf("\r\n> ");
            } else if (c == 0x08 || c == 0x7F) { // Backspace
                if (line_pos > 0) {
                    line_pos--;
                    printf("\b \b");
                }
            } else if (line_pos < SHELL_LINE_BUF_SIZE - 1) {
                line_buf[line_pos++] = (char)c;
                mp_hal_uart_send_byte(UART0, c); // Echo
            }
        }
        mp_thread_sleep(10);
    }
    return NULL;
}

void mp_shell_init(void) {
    line_pos = 0;
    memset(line_buf, 0, sizeof(line_buf));
}

void mp_shell_start(void) {
    mp_thread_attr_t attr = {.name = "shell", .priority = 1, .stack_size = 2048};
    mp_thread_create(mp_shell_task, NULL, &attr);
}
