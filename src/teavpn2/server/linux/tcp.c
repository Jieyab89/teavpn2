// SPDX-License-Identifier: GPL-2.0
/*
 *  src/teavpn2/server/linux/tcp.c
 *
 *  TeaVPN2 server core for Linux.
 *
 *  Copyright (C) 2021  Ammar Faizi
 */

#include <signal.h>
#include <stdlib.h>
#include <sys/epoll.h>

#include <teavpn2/net/linux/iface.h>
#include <teavpn2/server/linux/tcp.h>


#define CALCULATE_STATS		1
#define TUN_TAP_QUEUE_NUM	32u


struct srv_state {
	int			intr_sig;
	int			tcp_fd;
	int			epoll_fd;
	int			tun_fds[TUN_TAP_QUEUE_NUM];
	bool			stop_el;
	struct srv_cfg 		*cfg;
#if CALCULATE_STATS
	uint64_t		up_bytes;
	uint64_t		down_bytes;
#endif
};


static struct srv_state *g_state = NULL;


static void handle_interrupt(int sig)
{
	printf("\nInterrupt caught: %d\n", sig);

	if (g_state) {
		g_state->stop_el  = true;
		g_state->intr_sig = sig;
	} else {
		printf("Bug: handle_interrupt found that g_state is NULL\n");
		abort();
	}
}


static int init_state(struct srv_state *state)
{
	int ret = 0;
	struct srv_cfg *cfg = state->cfg;

	if (cfg->sys.thread == 0) {
		pr_err("Number of thread cannot be zero");
		return -EINVAL;
	}

	state->intr_sig     = -1;
	state->tcp_fd       = -1;
	state->epoll_fd     = -1;

	for (size_t i = 0; i < TUN_TAP_QUEUE_NUM; i++)
		state->tun_fds[i] = -1;

	state->stop_el      = false;
#if CALCULATE_STATS
	state->up_bytes     = 0ull;
	state->down_bytes   = 0ull;
#endif

	signal(SIGINT, handle_interrupt);
	signal(SIGTERM, handle_interrupt);
	signal(SIGHUP, handle_interrupt);
	signal(SIGPIPE, SIG_IGN);

	return ret;
}


static int init_iface(struct srv_state *state)
{
	int ret = 0;
	int tun_fd;
	int *tun_fds = state->tun_fds;


	prl_notice(3, "Allocating virtual network interface...");



	return ret;
}


int teavpn2_server_tcp(struct srv_cfg *cfg)
{
	int ret;
	struct srv_state state;

	memset(&state, 0, sizeof(state));

	state.cfg = cfg;
	g_state   = &state;

	ret = init_state(&state);
	if (unlikely(ret))
		goto out;

	ret = init_iface(&state);
	if (unlikely(ret))
		goto out;
out:
	return ret;
}
