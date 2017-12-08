/***
  This file is part of libdaemon.

  Copyright 2003-2008 Lennart Poettering

  libdaemon is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 2.1 of the
  License, or (at your option) any later version.

  libdaemon is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with libdaemon. If not, see
  <http://www.gnu.org/licenses/>.
***/

#include <signal.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/select.h>
#include <unistd.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <tcl.h>
#include "../lsctl/lsext.h"

#define LISTEN_BACKLOG 10

// TODO: Make it nicer, join with get_unix_socket
const char *get_socket_path()
{
	static char fn[256] = { 0 };
	static char buf[128];
	getcwd(buf, sizeof(buf));
	if (fn[0] == 0) {
		snprintf(fn, sizeof(fn), "%s/%s.socket", buf, daemon_pid_file_ident ? daemon_pid_file_ident : "unknown");
	}
	return fn;
}

int get_unix_socket()
{
	// TODO do it properly
	unlink(get_socket_path());

	struct sockaddr_un server_addr;

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sun_family = AF_UNIX;
	strncpy(server_addr.sun_path, get_socket_path(), sizeof(server_addr.sun_path) - 1);

	int s = socket(AF_UNIX, SOCK_STREAM, 0);

	if (bind(s, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) != 0)
		return -1;
	listen(s, LISTEN_BACKLOG);

	return s;
}

const char *get_name()
{
	static char fn[256] = { 0 };
	static char buf[128];
	getcwd(buf, sizeof(buf));
	if (fn[0] == 0) {
		snprintf(fn, sizeof(fn), "%s/%s.pid", buf, daemon_pid_file_ident ? daemon_pid_file_ident : "unknown");
	}
	return fn;
}

int main(int argc, char *argv[]) {
    pid_t pid;

    /* Reset signal handlers */
    if (daemon_reset_sigs(-1) < 0) {
        daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s", strerror(errno));
        return 1;
    }

    /* Unblock signals */
    if (daemon_unblock_sigs(-1) < 0) {
        daemon_log(LOG_ERR, "Failed to unblock all signals: %s", strerror(errno));
        return 1;
    }

    /* Set identification string for the daemon for both syslog and PID file */
    daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);

	get_name();
	daemon_pid_file_proc = get_name;

    /* Check if we are called with -k parameter */
    if (argc >= 2 && !strcmp(argv[1], "-k")) {
        int ret;

        /* Kill daemon with SIGTERM */

        /* Check if the new function daemon_pid_file_kill_wait() is available, if it is, use it. */
        if ((ret = daemon_pid_file_kill_wait(SIGTERM, 5)) < 0)
            daemon_log(LOG_WARNING, "Failed to kill daemon: %s", strerror(errno));

        return ret < 0 ? 1 : 0;
    }

    /* Check that the daemon is not rung twice a the same time */
    if ((pid = daemon_pid_file_is_running()) >= 0) {
        daemon_log(LOG_ERR, "Daemon already running on PID file %u", pid);
        return 1;
    }

    /* Prepare for return value passing from the initialization procedure of the daemon process */
    if (daemon_retval_init() < 0) {
        daemon_log(LOG_ERR, "Failed to create pipe.");
        return 1;
    }

    /* Do the fork */
//    if ((pid = daemon_fork()) < 0) {
//
//        /* Exit on error */
//        daemon_retval_done();
//        return 1;
//
//    } else if (pid) { /* The parent */
//        int ret;
//
//        /* Wait for 20 seconds for the return value passed from the daemon process */
//        if ((ret = daemon_retval_wait(20)) < 0) {
//            daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s", strerror(errno));
//            return 255;
//        }
//
//        daemon_log(ret != 0 ? LOG_ERR : LOG_INFO, "Daemon returned %i as return value.", ret);
//        return ret;
//
//    } else { /* The daemon */
        int fd, quit = 0;
        fd_set fds;

        /* Close FDs */
//        if (daemon_close_all(-1) < 0) {
//            daemon_log(LOG_ERR, "Failed to close all file descriptors: %s", strerror(errno));
//
//            /* Send the error condition to the parent process */
//            daemon_retval_send(1);
//            goto finish;
//        }

        /* Create the PID file */
        if (daemon_pid_file_create() < 0) {
            daemon_log(LOG_ERR, "Could not create PID file (%s).", strerror(errno));
            daemon_retval_send(2);
            goto finish;
        }

        /* Initialize signal handling */
        if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, SIGHUP, 0) < 0) {
            daemon_log(LOG_ERR, "Could not register signal handlers (%s).", strerror(errno));
            daemon_retval_send(3);
            goto finish;
        }

        /*... do some further init work here */
		/* Network daemon initialization follows */


        /* Send OK to parent process */
        daemon_retval_send(0);

        daemon_log(LOG_INFO, "Successfully started");

        /* Prepare for select() on the signal fd */
        FD_ZERO(&fds);
        fd = daemon_signal_fd();
		int fd2 = get_unix_socket();
        FD_SET(fd, &fds);
        FD_SET(fd2, &fds);

		int ret;
		int exitcode = 0;
		Tcl_FindExecutable(argv[0]);
		Tcl_Interp* interp = Tcl_CreateInterp();
		if (Tcl_Init(interp) != TCL_OK)
			return 1;

		register_lsdn_tcl(interp);
		printf("lsdn commands registered\n");

        while (!quit) {
            fd_set fds2 = fds;

            /* Wait for an incoming signal */
            if (select(FD_SETSIZE, &fds2, 0, 0, 0) < 0) {

                /* If we've been interrupted by an incoming signal, continue */
                if (errno == EINTR)
                    continue;

                daemon_log(LOG_ERR, "select(): %s", strerror(errno));
                break;
            }

            /* Check if a signal has been received */
            if (FD_ISSET(fd, &fds2)) {
                int sig;

                /* Get signal */
                if ((sig = daemon_signal_next()) <= 0) {
                    daemon_log(LOG_ERR, "daemon_signal_next() failed: %s", strerror(errno));
                    break;
                }

                /* Dispatch signal */
                switch (sig) {
                    case SIGINT:
                    case SIGQUIT:
                    case SIGTERM:
                        daemon_log(LOG_WARNING, "Got SIGINT, SIGQUIT or SIGTERM.");
                        quit = 1;
                        break;

                    case SIGHUP:
                        daemon_log(LOG_INFO, "Got a HUP");
                        daemon_exec("/", NULL, "/bin/ls", "ls", (char*) NULL);
                        break;

                }
            } else if (FD_ISSET(fd2, &fds2)) {
				struct sockaddr_un client_addr;
				socklen_t client_addr_size = sizeof(struct sockaddr_un);
				int fdc = accept(fd2, (struct sockaddr *) &client_addr, &client_addr_size);
				char buf[1024];
				ssize_t n = read(fdc, buf, 1024);

				if( (ret = Tcl_Eval(interp, buf)) != TCL_OK ) {
					Tcl_Obj *options = Tcl_GetReturnOptions(interp, ret);
					Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
					Tcl_Obj *stackTrace;
					Tcl_IncrRefCount(key);
					Tcl_DictObjGet(NULL, options, key, &stackTrace);
					Tcl_DecrRefCount(key);
					fprintf(stderr, "error: %s\n", Tcl_GetString(stackTrace));
					Tcl_DecrRefCount(options);
					exitcode = 1;
				}

				daemon_log(LOG_INFO, "Exit code = %d", exitcode);
				//write(fdc, "Closing...", 10);
				close(fdc);
			}
        }

		/* Do a cleanup */
		Tcl_Finalize();
finish:
        daemon_log(LOG_INFO, "Exiting...");
        daemon_retval_send(255);
        daemon_signal_done();
        daemon_pid_file_remove();

        return 0;
    }
//}
