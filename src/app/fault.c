/*
 * gtetrinet-ng — crash handling.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "fault.h"
#include "version.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GLIBC__)
#include <execinfo.h>
#include <sys/wait.h>
#include <unistd.h>
static void on_crash(int sig)
{
    void *frames[48];
    char head[96];
    int n = snprintf(head, sizeof head, "CRASH: %s %s, signal %d\n", TN_APP_NAME, TN_VERSION, sig);
    if (write(2, head, (size_t)n) < 0) {}
    int k = backtrace(frames, 48);
    backtrace_symbols_fd(frames, k, 2);
    signal(sig, SIG_DFL);
    _exit(128 + sig);
}
#elif defined(_WIN32)
#include <windows.h>
static LONG WINAPI on_exception(EXCEPTION_POINTERS *e)
{
    fprintf(stderr, "CRASH: %s %s, exception 0x%08lx at %p\n", TN_APP_NAME, TN_VERSION,
            (unsigned long)e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

void gtn_fault_install(void)
{
#ifdef __SANITIZE_ADDRESS__
    return; /* Preserve the sanitizer's own signal handler in diagnostic builds. */
#endif
#if defined(__GLIBC__)
    signal(SIGSEGV, on_crash);
    signal(SIGBUS, on_crash);
    signal(SIGFPE, on_crash);
    signal(SIGILL, on_crash);
    signal(SIGABRT, on_crash);
#elif defined(_WIN32)
    SetUnhandledExceptionFilter(on_exception);
#endif
}

/* Ordinary faults can safely start a helper; signal handlers only write diagnostics.
 * The launcher reports a crash after the process exits. Never upload detail/logs. */
static void report_fault(void)
{
    static int sent;
    if (!TN_FAULT_REPORTS || sent || (getenv("GTN_NO_REPORT") && !strcmp(getenv("GTN_NO_REPORT"), "1"))) return;
    sent = 1;
#ifdef _WIN32
    char command[] = "curl.exe -fsS --max-time 8 -d \"gtetrinet-ng " TN_VERSION " Windows application fault\" https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57";
    STARTUPINFOA start = {0};
    PROCESS_INFORMATION process = {0};
    start.cb = sizeof start;
    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &start, &process)) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
#elif defined(__GLIBC__)
    /* Double fork avoids accumulating zombies while the game stays open. */
    pid_t child = fork();
    if (child == 0) {
        if (fork() == 0) {
            execlp("curl", "curl", "-fsS", "--max-time", "8", "-o", "/dev/null", "-d",
                   "gtetrinet-ng " TN_VERSION " Linux application fault",
                   "https://ntfy.sh/gtetrinet-ng-faults-5e8b41e03cf04b57", (char *)NULL);
        }
        _exit(0);
    }
    if (child > 0) waitpid(child, NULL, 0);
#endif
}

void gtn_fault(const char *what, const char *detail)
{
    fprintf(stderr, "FAULT: %s: %s\n", what, detail ? detail : "");
    fflush(stderr);
    report_fault();
}
