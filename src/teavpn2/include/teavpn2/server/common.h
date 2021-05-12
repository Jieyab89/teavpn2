// SPDX-License-Identifier: GPL-2.0
/*
 *  src/teavpn2/include/teavpn2/server/common.h
 *
 *  Common header for TeaVPN2 server.
 *
 *  Copyright (C) 2021  Ammar Faizi
 */

#ifndef TEAVPN2__SERVER__COMMON_H
#define TEAVPN2__SERVER__COMMON_H

#include <teavpn2/base.h>


struct srv_sys_cfg {
	char		*data_dir;
	uint8_t		verbose_level;
	uint16_t	thread;
};


struct srv_sock_cfg {
	sock_type	type;
	char		*bind_addr;
	uint16_t	bind_port;
	uint16_t	max_conn;
	int		backlog;
	char		*ssl_cert;
	char		*ssl_priv_key;
};


struct srv_iface_cfg {
	char		*dev;
	uint16_t	mtu;
	char		*ipv4;
	char		*ipv4_netmask;
};


struct srv_cfg {
	struct srv_sys_cfg	sys;
	struct srv_sock_cfg	sock;
	struct srv_iface_cfg	iface;
};


int teavpn2_run_server(int argc, char *argv[]);
int teavpn2_argv_parse(int argc, char *argv[], struct srv_cfg *cfg);


#endif /* #ifndef TEAVPN2__SERVER__COMMON_H */
