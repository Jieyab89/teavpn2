// SPDX-License-Identifier: GPL-2.0
/*
 *  src/teavpn2/include/teavpn2/server/common.h
 *
 *  Common header for TeaVPN2 server.
 *
 *  Copyright (C) 2021  Ammar Faizi
 */

#ifndef TEAVPN2__NET__LINUX__IFACE_H
#define TEAVPN2__NET__LINUX__IFACE_H

#include <teavpn2/base.h>

int tun_alloc(const char *dev, short flags);
int fd_set_nonblock(int fd);
bool teavpn_iface_up(struct if_info *iface);
bool teavpn_iface_down(struct if_info *iface);


#endif /* #ifndef TEAVPN2__NET__LINUX__IFACE_H */
