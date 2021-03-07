
#if !defined(__linux__)
#  error "Compiler is not supported!"
#endif

#ifndef TEAVPN2__CLIENT__SOCK__TCP__CLIENT__LINUX_H
#define TEAVPN2__CLIENT__SOCK__TCP__CLIENT__LINUX_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <teavpn2/client/sock/tcp.h>

#define PIPE_BUF (16)

client_tcp_state *g_state;

#include "linux/init.h"
#include "linux/auth.h"
#include "linux/clean_up.h"

#define RACT(EXPR) ((state.stop) || (EXPR))

/**
 * @param client_cfg *config
 * @return int
 */
inline static int
__internal_tvpn_client_tcp_run(client_cfg *config)
{
  int               ret = 1; /* Exit code. */
  client_tcp_state  state;


  state.net_fd        = -1;
  state.tun_fd        = -1;
  state.pipe_fd[0]    = -1;
  state.pipe_fd[1]    = -1;
  state.is_authorized = false;
  state.stop          = false;
  state.config        = config;

  g_state             = &state;

  signal(SIGINT, tvpn_client_tcp_signal_handler);
  signal(SIGHUP, tvpn_client_tcp_signal_handler);
  signal(SIGTERM, tvpn_client_tcp_signal_handler);

  if (RACT(!tvpn_client_tcp_iface_init(&state))) {
    goto ret;
  }

  if (RACT(!tvpn_client_tcp_init_pipe(state.pipe_fd))) {
    goto ret;
  }

  if (RACT(!tvpn_client_tcp_sock_init(&state))) {
    goto ret;
  }

  if (RACT(!tvpn_client_tcp_auth(&state))) {
    goto ret;
  }

  /* TODO: Event loop here. */

ret:
  tvpn_client_tcp_clean_up(&state);
  return ret;
}

#endif /* #ifndef TEAVPN2__CLIENT__SOCK__TCP__CLIENT__LINUX_H */
