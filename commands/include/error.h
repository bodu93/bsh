#ifndef APUE3E_ERROR_H
#define APUE3E_ERROR_H

#include "base.h"
#include <stdarg.h>

/*
 * Print a message and return the caller.
 * Caller specifies "errnoflag"
 */
void err_doit(int errnoflag, int error, const char * fmt, va_list ap);

/*
 * Nofatal error related to a system call
 * Print a message and return
 */
void err_ret(const char *fmt, ...);

/*
 * Fatal error related to a system call
 * Print a message and terminate
 */
void err_sys(const char *fmt, ...);

/*
 * Nonfatal error unrelated to a system call
 * Error code passed as explicit parameter
 * Print a message and return.
 */
void err_cont(int error, const char *fmt, ...);

/*
 * Fatal error unrelated to a system call
 * Error code passed as explicit parameter
 * Print a message and terminate.
 */
void err_exit(int error, const char *fmt, ...);

/*
 * Fatal error related to a system call
 * Print a message, dump core, and terminate
 */
void err_dump(const char *fmt, ...);

/*
 * Nonfatal error unrelated to a system call
 * Print a message and return.
 */
void err_msg(const char *fmt, ...);

/*
 * Fatal error unrelated to asystem call.
 * Print a message and terminate.
 */
void err_quit(const char *fmt, ...);

#endif /* APUE3E_ERROR_H */
