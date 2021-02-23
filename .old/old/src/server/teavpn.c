
#include <stdio.h>
#include <unistd.h>

#include <teavpn2/global/iface.h>
#include <teavpn2/server/socket.h>
#include <teavpn2/server/common.h>
#include <teavpn2/server/socket/tcp.h>
#include <teavpn2/server/socket/udp.h>

static bool validate_config(teavpn_server_config *config);
static int teavpn_tcp_start(teavpn_server_config *config);

/**
 * @param teavpn_server_config *config
 * @return int
 */
int teavpn_server_run(teavpn_server_config *config)
{
  if (!validate_config(config)) {
    return 1;
  }

  int ret = 1;
  iface_info iinfo;

  debug_log(2, "Allocating teavpn interface...");
  iinfo.tun_fd = teavpn_iface_allocate(config->iface.dev);
  if (iinfo.tun_fd < 0) {
    return 1; /* No need to close tun_fd, since failed to create. */
  }

  debug_log(2, "Setting up teavpn network interface...");
  if (!teavpn_iface_init(&config->iface)) {
    error_log("Cannot set up teavpn network interface");
    ret = 1;
    goto close;
  }

  switch (config->socket_type) {
    case teavpn_sock_tcp:
      ret = teavpn_server_tcp_run(&iinfo, config);
      break;

    case teavpn_sock_udp:
      /* TODO: Make VPN be able to use UDP socket. */
      break;

    default:
      error_log("Invalid socket type");
      return 1;
      break;
  }

close:
  /* Close tun_fd. */
  debug_log(0, "Closing tun_fd...");
  close(iinfo.tun_fd);
  return ret;
}

/**
 * @param teavpn_server_config *config
 * @return bool
 */
static bool validate_config(teavpn_server_config *config)
{
  /**
   * Check data dir.
   */
  debug_log(5, "Checking data_dir...");
  if (config->data_dir == NULL) {
    error_log("Data dir cannot be empty!");
    return false;
  }

  return true;
}
