/*
 * gl_runner: start gl_test from a child of ours.
 *
 * launchd gives every job a 6 MB per-task jetsam limit on this port (measured:
 * a stock driverkitd and gl_test alike die with JETSAM_REASON_MEMORY_PERPROCESSLIMIT
 * and memlimit 6 MB), which a 16 MB framework cannot fit in. A forked child gets
 * its own task and ledger, so the limit launchd set on this job's task does not
 * follow it. This is how the reference runs its GL programs too (a tiny init
 * forking the job script's commands).
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int
main(int argc, char **argv)
{
	const char *prog = argc > 1 ? argv[1] : "/private/var/gl_test";
	pid_t pid = fork();

	if (pid == 0) {
		execv(prog, (char *const[]){ (char *)prog, "30", "256", "256", NULL });
		perror("gl_runner: execv");
		_exit(127);
	}
	if (pid < 0) {
		perror("gl_runner: fork");
		return 1;
	}
	int status = 0;
	waitpid(pid, &status, 0);
	return 0;
}
