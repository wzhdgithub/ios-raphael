/*
 * libsweetmark: says when dyld has loaded it into a program.
 *
 * The kernel's kern.exec_inject adds DYLD_INSERT_LIBRARIES to the environment of
 * the programs named in the boot-arg; this library is how that is checked before
 * the window server gets the real thing (libhakios.gl.dylib). It prints to stderr,
 * which launchd's jobs here have on the console, so the line lands in sweetlog.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__attribute__((constructor))
static void
sweet_mark(void)
{
	fprintf(stderr, "SWEET MARK: libsweetmark loaded in %d (%s)\n", (int)getpid(), getprogname());
}
