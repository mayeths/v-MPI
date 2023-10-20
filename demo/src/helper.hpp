#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>

#define HELPER_PUBLIC   __attribute__((noinline))
#define HELPER_INTERNAL __attribute__((noinline)) static

extern "C" {

////////////
//////////// Public API
////////////

HELPER_PUBLIC int establish_ssh_forward_tunnel(int local_port, int remote_port, const char *remote_host, const char *remote_user);
HELPER_PUBLIC void helper_main_no_return(pid_t master_pid, int local_port, int remote_port, const char *remote_host, const char *remote_user);

////////////
//////////// Internal Implementation
////////////

#define HANDLE_MAX 64        // maximum signal to handle
#define LARGE_BUF_SIZE 65536 // for holding the entire backtrace
#define SMALL_BUF_SIZE 512   // for holding small message
#define SMALL_BUF_NUM  1     // message
#define RESERVED_SIZE (256 * 1024)
#define STACK_SIZE ( \
    (SIGSTKSZ) + \
    (LARGE_BUF_SIZE) + \
    (SMALL_BUF_SIZE * SMALL_BUF_NUM) + \
    (RESERVED_SIZE) \
)

HELPER_INTERNAL void _helper_main_loop();
HELPER_INTERNAL void _helper_cleanup();
HELPER_INTERNAL void _helper_signal_handler(int signo, siginfo_t *info, void *context);

struct _helper_G_t {
    pid_t master_pid;
    uint8_t *stack_memory;
    mys_popen_t tunnel;
    int nsig;
    int sigs[_HANDLE_MAX];
    struct sigaction old_actions[_HANDLE_MAX];
};
static struct _helper_G_t *_helper_G = NULL;

HELPER_PUBLIC int establish_ssh_forward_tunnel(int local_port, int remote_port, const char *remote_host, const char *remote_user)
{
    pid_t parent_pid = getpid();
    pid_t child_pid = fork();
    AS_NE_I32(child_pid, -1);

    if (child_pid == 0) {
        helper_main_no_return(parent_pid, local_port, remote_port, remote_host, remote_user);
    } else {
        printf("MASTER-%d: Return to user\n", (int)getpid());
    }

    return child_pid;
}

HELPER_PUBLIC void helper_main_no_return(pid_t master_pid, int local_port, int remote_port, const char *remote_host, const char *remote_user)
{
    struct sigaction new_action, old_action;
    stack_t stack;
    char message[SMALL_BUF_SIZE];
    memset(message, 0, sizeof(message));

#define FINISH_IF(exp, fmt, ...) do {                                     \
    if (exp) {                                                            \
        std::snprintf(message, sizeof(message), fmt "\n", ##__VA_ARGS__); \
        goto finished;                                                    \
    }                                                                     \
} while (0)

    _helper_G = (struct _helper_G_t *)malloc(sizeof(struct _helper_G_t));
    FINISH_IF(_helper_G == NULL, "Allocate memory for _helper_G failed");
    memset(_helper_G, 0, sizeof(struct _helper_G_t));
    _helper_G->master_pid = master_pid;
    _helper_G->tunnel.pid = 0;

    {
        _helper_G->stack_memory = (uint8_t *)malloc(STACK_SIZE);
        FINISH_IF(_helper_G->stack_memory == NULL, "Allocate memory for signal stack failed");
        memset(_helper_G->stack_memory, 0, STACK_SIZE);
        stack.ss_sp = _helper_G->stack_memory;
        stack.ss_size = STACK_SIZE;
        stack.ss_flags = 0;
        FINISH_IF(sigaltstack(&stack, NULL) == -1, "Register signal stack failed");

        new_action.sa_sigaction = _helper_signal_handler;
        new_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&new_action.sa_mask);

        _helper_G->nsig = 0;
        _helper_G->sigs[_helper_G->nsig++] = SIGINT;
        // ... Add more signals
        FINISH_IF(_helper_G->nsig > HANDLE_MAX, "Too many signals to handle");

        for (int i = 0; i < _helper_G->nsig; ++i) {
            int sig = _helper_G->sigs[i];
            _helper_G->old_actions[i].sa_sigaction = NULL;
            FINISH_IF(sigaction(sig, &new_action, &_helper_G->old_actions[i]) == -1, "Failed to set handler for signal %d", sig);
        }
    }

    std::snprintf(message, sizeof(message), "ssh -L %d:localhost:%d %s@%s",
        local_port, remote_port, remote_user, remote_host);
    _helper_G->tunnel = mys_popen_create(message);
    memset(message, 0, sizeof(message));
    FINISH_IF(!_helper_G->tunnel.valid, "Failed to fork a SSH process");

    _helper_main_loop();

finished:
    if (message[0] != '\0') {
        size_t len = strnlen(message, sizeof(message));
        write(STDOUT_FILENO, message, len);
    }
    _helper_cleanup();
    exit(0);
#undef FINISH_IF
}

HELPER_INTERNAL void _helper_main_loop()
{
    bool finished = false;
    while (!finished)
    {
        if (getppid() == _helper_G->master_pid) {
            usleep(500 * 1000);
        } else {
            finished = 1;
        }
    }
}

HELPER_INTERNAL void _helper_cleanup()
{
    if (_helper_G == NULL)
        return;

    if (_helper_G->tunnel.valid)
        mys_popen_kill(&_helper_G->tunnel, SIGABRT);
}

HELPER_INTERNAL void _helper_signal_handler(int signo, siginfo_t *info, void *context)
{

}

} /* extern "C"*/
