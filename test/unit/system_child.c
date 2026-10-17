#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void repeated(FILE *stream, int byte, size_t n)
{
	char block[4096];
	memset(block, byte, sizeof(block));
	while (n) {
		size_t part = n > sizeof(block) ? sizeof(block) : n;
		if (fwrite(block, 1, part, stream) != part)
			exit(90);
		n -= part;
	}
	fflush(stream);
}
int main(int argc, char **argv)
{
	if (argc < 2)
		return 89;
	if (!strcmp(argv[1], "duplex")) {
		repeated(stdout, 'O', 131072);
		repeated(stderr, 'E', 131072);
		char data[4096];
		size_t n;
		while ((n = fread(data, 1, sizeof(data), stdin)))
			if (fwrite(data, 1, n, stdout) != n)
				return 91;
		return 0;
	}
	if (!strcmp(argv[1], "limit")) {
		repeated(stdout, 'O', 100000);
		repeated(stderr, 'E', 100000);
		return 0;
	}
	if (!strcmp(argv[1], "descriptor"))
		return argc > 2 && fcntl(atoi(argv[2]), F_GETFD) < 0 ? 0 : 96;
	if (!strcmp(argv[1], "exit"))
		return argc > 2 ? atoi(argv[2]) : 0;
	if (!strcmp(argv[1], "argument")) {
		if (argc > 2)
			fwrite(argv[2], 1, strlen(argv[2]), stdout);
		return 0;
	}
	if (!strcmp(argv[1], "signal")) {
		raise(SIGTERM);
		return 93;
	}
	if (!strcmp(argv[1], "ignore")) {
		signal(SIGTERM, SIG_IGN);
		if (argc > 2) {
			int fd = open(argv[2], O_CREAT | O_WRONLY, 0600);
			if (fd >= 0)
				close(fd);
		}
		for (;;)
			pause();
	}
	if (!strcmp(argv[1], "descendant")) {
		if (!fork()) {
			signal(SIGTERM, SIG_IGN);
			for (;;)
				pause();
		}
		return 0;
	}
	if (!strcmp(argv[1], "environment")) {
		char cwd[4096];
		if (!getcwd(cwd, sizeof(cwd)))
			return 94;
		const char *value = getenv("TAPAS_SYSTEM_TEST");
		printf("%s\n%s", cwd, value ? value : "missing");
		return 0;
	}
	return 95;
}
