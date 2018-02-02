#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <tcl.h>

void usage(const char *pname) {
	fprintf(stderr, "usage: %s CONTROL-SOCKET [COMMAND TOKENS]\n", pname);
	fprintf(stderr,
		"Sends commands to a running lsctld instance. Commands can be taken from commandline \n"
		"or from stdin. \n");
	exit(1);
}

int open_sockaddr(const char *path)
{
	struct sockaddr_un server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, path, sizeof(server_addr.sun_path) - 1);

	int s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		return -1;

	if (connect(s, (const struct sockaddr*) &server_addr, sizeof(server_addr)) != 0)
		return -1;

	return s;
}

typedef struct {
	size_t size;
	size_t used;
	size_t pos;
	char *data;
} buf;

static void buf_reset(buf *b)
{
	b->pos = 0;
	b->used = 0;
}

static int buf_write(buf *b, int fd)
{
	int r = write(fd, b->data + b->pos, b->used - b->pos);
	if (r > 0)
		b->pos += r;
	if (b->pos >= b->used)
		buf_reset(b);
	return r;
}

static int buf_read(buf *b, int fd)
{
	int r = read(fd, b->data, b->size);
	if (r < 0)
		return r;
	b->used = r;
	return r;
}

#define BUFSIZE 4096

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Expected at least one argument\n");
		usage(argv[0]);
	}

	bool cmd_on_cmdline = argc > 2;
	bool input_closed = cmd_on_cmdline;
	bool socket_closed = false;
	int stdin_fd = 0;
	int stdout_fd = 1;
	int sock_fd = open_sockaddr(argv[1]);
	if (sock_fd < 0) {
		perror("open socket");
		return 1;
	}

	/* Prepare the buffers */
	buf sendbuf, rcvbuf;
	if (cmd_on_cmdline) {
		sendbuf.data = Tcl_Merge(argc - 2, (const char**) &argv[2]);
		sendbuf.used = strlen(sendbuf.data);
		sendbuf.size = sendbuf.used + 1;
	} else {
		sendbuf.data = malloc(BUFSIZE);
		if (!sendbuf.data)
			abort();
		sendbuf.used = 0;
		sendbuf.size = BUFSIZE;
	}
	sendbuf.pos = 0;
	rcvbuf.data = malloc(BUFSIZE);
	if (!rcvbuf.data)
		abort();
	rcvbuf.used = 0;
	rcvbuf.size = BUFSIZE;
	rcvbuf.pos = 0;

	int ret = 0;
	int r;
	while (! (socket_closed && rcvbuf.used == 0)) {
		fd_set wrset, rdset;
		FD_ZERO(&wrset);
		FD_ZERO(&rdset);

		if (rcvbuf.used) {
			FD_SET(stdout_fd, &wrset);
		} else if (!socket_closed){
			FD_SET(sock_fd, &rdset);
		}

		if (sendbuf.used) {
			FD_SET(sock_fd, &wrset);
		} else if (!input_closed) {
			FD_SET(stdin_fd, &rdset);
		}

		if ((r = select(10, &rdset, &wrset, NULL, NULL)) < 0) {
			perror("select");
			ret = 1;
			break;
		}

		if (rcvbuf.used) {
			if (FD_ISSET(stdout_fd, &wrset)) {
				if ((r = buf_write(&rcvbuf, stdout_fd)) < 0) {
					perror("write");
					ret = 1;
					break;
				}
			}
		} else if (!socket_closed){
			if (FD_ISSET(sock_fd, &rdset)) {
				if ((r = buf_read(&rcvbuf, sock_fd)) < 0) {
					perror("read");
					ret = 1;
					break;
				} else if (r == 0) {
					socket_closed = true;
				}
			}
		}

		if (sendbuf.used) {
			if (FD_ISSET(sock_fd, &wrset)) {
				if ((r = buf_write(&sendbuf, sock_fd)) < 0) {
					perror("write");
					ret = 1;
					break;
				}
				if (cmd_on_cmdline && !sendbuf.used) {
					shutdown(sock_fd, SHUT_WR);
				}
			}
		} else if (!input_closed) {
			if (FD_ISSET(stdin_fd, &rdset)) {
				if ((r = buf_read(&sendbuf, stdin_fd)) < 0) {
					perror("read");
					ret = 1;
					break;
				} else if (r == 0) {
					input_closed = true;
					shutdown(sock_fd, SHUT_WR);
				}
			}
		}

	}

	free(rcvbuf.data);
	if (cmd_on_cmdline)
		Tcl_Free(sendbuf.data);
	else
		free(sendbuf.data);

	return ret;
}
