
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <linux/ip.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <teavpn2/global/iface.h>
#include <teavpn2/server/common.h>
#include <teavpn2/server/socket/tcp.h>
#include <teavpn2/global/data_struct.h>

#define RECV_BUFFER 4096
#define TUN_READ_SIZE 4096
#define MAX_CLIENT_CHANNEL 10
#define TUN_MAX_READ_ERROR 1000
#define CLIENT_MAX_WRITE_ERROR 1000
#define MIN_WAIT_RECV_BYTES (sizeof(teavpn_cli_pkt))
#define CLI_PKT(A) ((teavpn_cli_pkt *)(A.cli_pkt_arena))
#define SRV_PKT(A) ((teavpn_srv_pkt *)(A.srv_pkt_arena))
#define CLI_PKT_P(A) ((teavpn_cli_pkt *)(A->cli_pkt_arena))
#define SRV_PKT_P(A) ((teavpn_srv_pkt *)(A->srv_pkt_arena))
#define MZERO_HANDLE(FUNC, RETVAL, ACT) if (RETVAL == 0) { ACT; }
#define MERROR_HANDLE(FUNC, RETVAL, ACT) if (RETVAL < 0) { perror("Error "#FUNC); ACT; }
#define RECV_ZERO_HANDLE(RETVAL, ACT) MZERO_HANDLE(recv(), RETVAL, ACT)  
#define SEND_ZERO_HANDLE(RETVAL, ACT) MZERO_HANDLE(send(), RETVAL, ACT)
#define READ_ZERO_HANDLE(RETVAL, ACT) MZERO_HANDLE(read(), RETVAL, ACT)
#define WRITE_ZERO_HANDLE(RETVAL, ACT) MZERO_HANDLE(write(), RETVAL, ACT)
#define RECV_ERROR_HANDLE(RETVAL, ACT) MERROR_HANDLE(recv(), RETVAL, ACT)  
#define SEND_ERROR_HANDLE(RETVAL, ACT) MERROR_HANDLE(send(), RETVAL, ACT)
#define READ_ERROR_HANDLE(RETVAL, ACT) MERROR_HANDLE(read(), RETVAL, ACT)
#define WRITE_ERROR_HANDLE(RETVAL, ACT) MERROR_HANDLE(write(), RETVAL, ACT)

typedef struct {
  bool is_online;
  bool is_authenticated;

  pthread_t thread;
  int fd;

  /* Readable client address and port. */
  struct {
    char addr[32];
    uint8_t addr_len;
    uint16_t port;
  } rdb;

  struct {
    char username[255];
    uint8_t username_len;
  };

  char srv_pkt_arena[4096 + 1024];
  char cli_pkt_arena[4096 + 1024];

  ssize_t signal_rlen;
  ssize_t slen;

  uint16_t write_error_count;

  struct sockaddr_in client_addr;

} teavpn_tcp_channel;

static int tun_fd;
static int net_fd;
static bool stop_main = false;
static teavpn_server_config *config;
static int16_t free_chan_index = 0; /* Set -1 if the channel array is full. */
static teavpn_tcp_channel *channels;

static bool teavpn_server_tcp_init(struct sockaddr_in *server_addr);
static bool teavpn_server_tcp_socket_setup();
static void teavpn_server_tcp_accept_client();
static void *teavpn_server_tcp_host_dispatcher(void *p);
static void *teavpn_server_tcp_serve_client(teavpn_tcp_channel *chan);
static int teavpn_server_tcp_client_wait(teavpn_tcp_channel *chan);
static bool teavpn_server_tcp_client_auth(teavpn_tcp_channel *chan);
static void teavpn_server_tcp_handle_client_data(teavpn_tcp_channel *chan);

/**
 * @param teavpn_server_config *config
 * @return bool
 */
int teavpn_server_tcp_run(iface_info *iinfo, teavpn_server_config *_config)
{
  int ret;
  pthread_t dispatcher_thread;
  struct sockaddr_in server_addr;
  teavpn_tcp_channel _channels[MAX_CLIENT_CHANNEL] = {0};

  bzero(_channels, sizeof(channels));

  config = _config;
  channels = _channels;
  tun_fd = iinfo->tun_fd;

  if (!teavpn_server_tcp_init(&server_addr)) {
    ret = 1;
    goto close;
  }


  /* Create the thread that is responsible to read and broadcast tun data.  */
  pthread_create(&dispatcher_thread, NULL, teavpn_server_tcp_host_dispatcher, NULL);
  pthread_detach(dispatcher_thread);

  while (true) {
    teavpn_server_tcp_accept_client();
    if (stop_main) {
      goto close;
    }
  }


close:
  /* Close main TCP socket fd. */
  if (net_fd != -1) {
    close(net_fd);
  }
  return ret;
}

/**
 * @param struct sockaddr_in *server_addr
 * @return bool
 */
static bool teavpn_server_tcp_init(struct sockaddr_in *server_addr)
{
  /**
   * Create TCP socket.
   */
  debug_log(3, "Creating TCP socket...");
  net_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (net_fd < 0) {
    error_log("Cannot create TCP socket");
    perror("Socket creation failed");
    return false;
  }
  debug_log(4, "TCP socket created successfully");

  /**
   * Setup TCP socket.
   */
  debug_log(3, "Setting up socket file descriptor...");
  if (!teavpn_server_tcp_socket_setup()) {
    return false;
  }
  debug_log(4, "Socket file descriptor set up successfully");

  /**
   * Prepare server bind address data.
   */
  bzero(server_addr, sizeof(struct sockaddr_in));
  server_addr->sin_family = AF_INET;
  server_addr->sin_port = htons(config->socket.bind_port);
  server_addr->sin_addr.s_addr = inet_addr(config->socket.bind_addr);

  /**
   * Bind socket to corresponding address.
   */
  if (bind(net_fd, (struct sockaddr *)server_addr, sizeof(struct sockaddr_in)) < 0) {
    error_log("Bind socket failed");
    perror("Bind failed");
    return false;
  }

  /**
   * Listen.
   */
  if (listen(net_fd, 3) < 0) {
    error_log("Listen socket failed");
    perror("Listen failed");
    return false;
  }

  /**
   * Ignore SIGPIPE
   */
  signal(SIGPIPE, SIG_IGN);

  debug_log(4, "Listening on %s:%d...", config->socket.bind_addr, config->socket.bind_port);

  return true;
}

/**
 * @return bool
 */
static bool teavpn_server_tcp_socket_setup()
{
  int optval = 1;
  if (setsockopt(net_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)) < 0) {
    perror("setsockopt()");
    return false;
  }
  return true;
}

/**
 * @return void
 */
static void teavpn_server_tcp_accept_client()
{
  int client_fd;
  teavpn_tcp_channel *chan;
  struct sockaddr_in client_addr;
  socklen_t rlen = sizeof(struct sockaddr_in);

  if (free_chan_index == -1) {
    goto prepare_free_channel;
  }

  /**
   * Accepting client connection.
   */
  chan = &(channels[free_chan_index]);
  client_fd = accept(net_fd, (struct sockaddr *)&(chan->client_addr), &rlen);
  MERROR_HANDLE(accept(), client_fd,
    {
      debug_log(1, "An error occured when accepting connection!");
    }
  );

  chan->fd = client_fd;
  chan->is_online = true;
  chan->is_authenticated = false;

  /* Copy readable address and port. */
  chan->rdb.port = chan->client_addr.sin_port;
  strcpy(chan->rdb.addr, inet_ntoa(chan->client_addr.sin_addr));
  chan->rdb.addr_len = strlen(chan->rdb.addr);

  debug_log(1, "Accepting client (%s:%d)...", chan->rdb.addr, chan->rdb.port);

  /* Create a new thread to serve the client. */
  pthread_create(&(chan->thread), NULL, (void * (*)(void *))teavpn_server_tcp_serve_client, chan);
  pthread_detach(chan->thread);

  free_chan_index = -1;

prepare_free_channel:
  /* Prepare free channel for next accept. */
  debug_log(5, "Preparing free channel for next accept...");
  for (register uint16_t i = 0; i < MAX_CLIENT_CHANNEL; i++) {
    if (!channels[i].is_online) {
      debug_log(5, "Got free channel at index %d", i);
      free_chan_index = i;
      break;
    }
  }

  /* Don't accept new connection if channel is full. */
  if (free_chan_index == -1) {
    debug_log(5, "Channel is full, sleep 1 second and try again.");
    sleep(1);
    goto prepare_free_channel;
  }
}

/**
 * @param void *p
 * @return void *
 */
static void *teavpn_server_tcp_host_dispatcher(void *p)
{
  #define srv_pkt ((teavpn_srv_pkt *)arena)
  #define send_size (sizeof(teavpn_srv_pkt) + srv_pkt->len - 1)

  char arena[sizeof(teavpn_srv_pkt) + 4096];
  ssize_t nread, slen;
  uint16_t read_fails = 0;

  srv_pkt->type = SRV_PKT_DATA;

  while (true) {

    /**
     * Read from TUN.
     */
    nread = read(tun_fd, srv_pkt->data, TUN_READ_SIZE);
    READ_ERROR_HANDLE(nread, {
      read_fails++;

      if (read_fails >= TUN_MAX_READ_ERROR) {
        stop_main = true;
        break;
      }

      continue;
    });

    srv_pkt->len = (uint16_t)nread;
    debug_log(5, "Read from tun_fd: %d bytes", srv_pkt->len);


    for (register int16_t i = 0; i < MAX_CLIENT_CHANNEL; i++) {
      /* Send data to all authenticated clients. */
      if (channels[i].is_online && channels[i].is_authenticated) {
        slen = send(channels[i].fd, srv_pkt, send_size, 0);
        SEND_ERROR_HANDLE(slen,
          {
            channels[i].write_error_count++;

            if (channels[i].write_error_count >= CLIENT_MAX_WRITE_ERROR) {
              channels[i].is_online = false;
              continue;
            }
          }
        );
      }
    }

    if (read_fails > 0) {
      read_fails--;
    }
  }

  #undef srv_pkt
  #undef send_size
  return NULL;
}

/**
 * @param teavpn_tcp_channel *chan
 * @return void *
 */
static void *teavpn_server_tcp_serve_client(teavpn_tcp_channel *chan)
{
  uint16_t wait_fails = 0;

  while (true) {

    if (!chan->is_online) {
      goto close_client;
    }

    if (teavpn_server_tcp_client_wait(chan) == -1) {
      wait_fails++;

      debug_log(5, "[%s:%d] wait_fails got increased %d",
        chan->rdb.addr, chan->rdb.port, wait_fails);

      if (wait_fails >= 1000) {
        debug_log(3, "[%s:%d] Too many wait_fails", chan->rdb.addr, chan->rdb.port);
        goto close_client;
      }

      continue;
    }

    SRV_PKT_P(chan)->type = SRV_PKT_AUTH_REQUIRED;
    chan->slen = send(chan->fd, SRV_PKT_P(chan), sizeof(teavpn_srv_pkt), 0);
    SEND_ERROR_HANDLE(chan->slen, {
      goto close_client;
    });

    switch (CLI_PKT_P(chan)->type) {

      case CLI_PKT_AUTH:
        if (!teavpn_server_tcp_client_auth(chan)) {
          goto close_client;
        }
        break;

      case CLI_PKT_DATA:
        teavpn_server_tcp_handle_client_data(chan);
        break;

      default:
        debug_log(5, "[%s:%d] Got unknown packet", chan->rdb.addr, chan->rdb.port);
        break;
    }

    if (wait_fails > 0) {
      wait_fails--;
    }
  }

close_client:
  debug_log(2, "[%s:%d] Closing connection...", chan->rdb.addr, chan->rdb.port);
  close(chan->fd);
  bzero(chan, sizeof(teavpn_tcp_channel));
  return NULL;
}

/**
 * @param teavpn_tcp_channel *chan
 * @return int
 */
static int teavpn_server_tcp_client_wait(register teavpn_tcp_channel *chan)
{
  register ssize_t recv_ret;
  register uint16_t total_recv_bytes = 0;

  while (total_recv_bytes < MIN_WAIT_RECV_BYTES) {

    recv_ret = recv(
      chan->fd,
      &(((char *)CLI_PKT_P(chan))[total_recv_bytes]),
      RECV_BUFFER,
      0
    );

    /* Error occured when calling recv. */
    RECV_ERROR_HANDLE(recv_ret, return -1);

    /* Client has been disconnected. */
    RECV_ZERO_HANDLE(recv_ret, {
      chan->is_online = false;
      debug_log(5,
        "[%s:%d] Got zero byte read (assuming as disconnected)",
        chan->rdb.addr, chan->rdb.port);
      return -1;
    });


    total_recv_bytes += (uint16_t)recv_ret;
  }

  return (int)total_recv_bytes;
}

/**
 * @param teavpn_tcp_channel *chan
 * @return bool
 */
static bool teavpn_server_tcp_client_auth(teavpn_tcp_channel *chan)
{

}

/**
 * @param teavpn_tcp_channel *chan
 * @return void
 */
static void teavpn_server_tcp_handle_client_data(teavpn_tcp_channel *chan)
{

}
