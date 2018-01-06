#include "../include/error.h"

/* for errno */
#include <errno.h>
/* for strcat, strlen */
#include <string.h>
/* for vsnprintf, snprintf, fputs, fflush, strerror */
#include <stdio.h>
/* for abort, exit */
#include <stdlib.h>

void
err_doit(int errnoflag, int error, const char *fmt, va_list ap) {
	char buf[MAXLINE];

	vsnprintf(buf, MAXLINE - 1, fmt, ap);
	if (errnoflag) {
		snprintf(buf + strlen(buf), MAXLINE - strlen(buf - 1), ": %s", strerror(error));
	}

	strcat(buf, "\n");
	fflush(stdout);
	fputs(buf, stderr);
	fflush(NULL);
}

void
err_ret(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
}

void
err_sys(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
	exit(1);
}

void
err_cont(int error, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, error, fmt, ap);
	va_end(ap);
}

void
err_exit(int error, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, error, fmt, ap);
	va_end(ap);
	exit(1);
}

void
err_dump(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
	abort();	/* dump core and terminate */
	exit(1);
}

void
err_msg(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(0, 0, fmt, ap);
	va_end(ap);
}

void
err_quit(const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	err_doit(0, 0, fmt, ap);
	va_end(ap);
	exit(1);
}
