#include "esp_log.h"
#include "lwip/sockets.h"
#include "ev.h"
#include "http_parser.h"

#include "wifi.h"

#define MAX_BACKLOG 8
#define BUFSIZE 128

static const char TAG[] = "main";

static int nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl");
    return -1;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)  {
    perror("fcntl");
    return -1;
  }
  return 0;
}

static int create_server_socket() {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    goto error;
  }

  struct sockaddr_in saddr;
  memset(&saddr, 0, sizeof(saddr));
  saddr.sin_family = PF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(80);
  if (bind(fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
    perror("bind");
    goto error;
  }

  if (listen(fd, MAX_BACKLOG) < 0) {
    goto error;
  }

  return fd;

error:
  if (0 <= fd) {
    close(fd);
  }
  return -1;
}

struct parser_data {
  int fd;
  bool message_complete;
  bool keepalive;
};

static void parser_data_reset(struct parser_data *data) {
  data->message_complete = false;
  data->keepalive = true;
}

static void parser_data_init(struct parser_data *data, int fd) {
  parser_data_reset(data);
  data->fd = fd;
}

static int on_message_begin(struct http_parser *parser) {
  struct parser_data *data = (struct parser_data*)parser->data;
  parser_data_reset(data);
  return 0;
}

static int on_url(struct http_parser *parser, const char *buf, size_t len) {
  // TODO parse url and you may store result on parser_data
  //struct parser_data *data = (struct parser_data*)parser->data;
  return 0;
}

static int on_body(struct http_parser *parser, const char *buf, size_t len) {
  // TODO parse body
  //struct parser_data *data = (struct parser_data*)parser->data;
  return 0;
}

static int on_message_complete(struct http_parser *parser) {
  struct parser_data *data = (struct parser_data*)parser->data;
  data->message_complete = true;
  data->keepalive = http_should_keep_alive(parser);

  // TODO switch by result of on_url
  const char *response = NULL;
  if (data->keepalive) {
    response = "HTTP/1.1 200 OK\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
  } else {
    response = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
  }
  size_t len = strlen(response);
  // TODO partial send may not occur because response is shorter than send buffer size.
  // If your response is very long, check partial send and retry on libev context :)
  write(data->fd, response, len);
  return 0;
}

static struct http_parser_settings settings;

void recv_event(EV_P_ struct ev_io *client_watcher, int revents) {
  char buf[BUFSIZE];
  struct http_parser *parser = (struct http_parser*)client_watcher->data;
  struct parser_data *data = (struct parser_data*)parser->data;
  while (1) {
    ssize_t nread = read(client_watcher->fd, buf, BUFSIZE);
    if (nread < 0) {
      if (errno == EINTR) { continue; }
      if (errno == EAGAIN || errno == EWOULDBLOCK) { return; }
      perror("recv");
    }
    if (nread <= 0) {
      goto end;
    }
    size_t nparsed = http_parser_execute(parser, &settings, buf, nread);
    if (nparsed != nread) {
      ESP_LOGI(TAG, "invalid request");
      goto end;
    }
    if (!data->keepalive) {
      goto end;
    }
  }
  return;

end:
  close(client_watcher->fd);
  ev_io_stop(EV_A_ client_watcher);
  free(parser->data);
  free(parser);
  free(client_watcher);
}

static void accept_event(EV_P_ struct ev_io *server_watcher, int revents) {
  struct sockaddr_in caddr;
  socklen_t caddr_len = sizeof(caddr);
  struct ev_loop *l;
  ev_io *client_watcher;
  struct http_parser *parser = NULL;

  int client_fd = accept(server_watcher->fd, (struct sockaddr *) &caddr, &caddr_len);
  if (client_fd < 0) {
    if (EINTR == errno) { return; }
    perror("accept");
    goto error;
  }
  if (nonblocking(client_fd) < 0) {
    goto error;
  }

  client_watcher = calloc(1, sizeof(ev_io));
  if (!client_watcher) {
    perror("calloc");
    goto error;
  }

  client_watcher->data = parser = malloc(sizeof(struct http_parser));
  if (!parser) {
    perror("malloc");
    goto error;
  }
  http_parser_init(parser, HTTP_REQUEST);
  parser->data = malloc(sizeof(struct parser_data));
  if (!parser->data) {
    perror("malloc");
    goto error;
  }
  parser_data_init(parser->data, client_fd);
  http_parser_settings_init(&settings);
  settings.on_message_begin = on_message_begin;
  settings.on_url = on_url;
  settings.on_body = on_body;
  settings.on_message_complete = on_message_complete;

  l = server_watcher->data;
  ev_io_init(client_watcher, recv_event, client_fd, EV_READ);
  ev_io_start(l, client_watcher);
  return;

error:
  if (0 <= client_fd) {
    close(client_fd);
  }
  ev_io_stop(EV_A_ server_watcher);
  ev_unloop(EV_A_ EVUNLOOP_ALL);
  free(parser);
}

void app_main() {
  wifi_init();
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

  struct ev_loop *loop;
  ev_io server_watcher;

  int server_fd = create_server_socket();
  if (server_fd < 0) {
    ESP_LOGE(TAG, "failed to create socket");
    return;
  }

  loop = ev_default_loop(0);
  server_watcher.data = loop;

  ev_io_init(&server_watcher, accept_event, server_fd, EV_READ);
  ev_io_start(loop, &server_watcher);

  ev_loop(loop, 0);
}
