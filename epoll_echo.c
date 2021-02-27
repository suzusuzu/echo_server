#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG_SIZE 5
#define BUF_SIZE 1024
#define N_CLIENT 256
#define INF_TIME -1
#define DISABLE -1

int listen_fd;

void int_handle(int n) {
  close(listen_fd);
  exit(EXIT_SUCCESS);
}

// wirte n byte
ssize_t write_n(int fd, char *ptr, size_t n) {
  ssize_t n_left = n, n_written;

  while (n_left > 0) {
    if ((n_written = write(fd, ptr, n_left)) <= 0) {
      return n_written;
    }
    n_left -= n_written;
    ptr += n_written;
  }

  return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
  char buf[BUF_SIZE];

  // Create listen socket
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error: socket\n");
    return EXIT_FAILURE;
  }

  // TCP port number
  int port = 8080;

  // Initialize server socket address
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(port);

  // Bind socket to an address
  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    fprintf(stderr, "Error: bind\n");
    return EXIT_FAILURE;
  }

  // Listen
  if (listen(listen_fd, BACKLOG_SIZE) < 0) {
    fprintf(stderr, "Error: listen\n");
    return EXIT_FAILURE;
  }

  // Set INT signal handler
  signal(SIGINT, int_handle);

  fprintf(stderr, "listen on port %d\n", port);

  // Create epoll
  int epfd = epoll_create1(0);
  if (epfd < 0) {
    fprintf(stderr, "Error: epoll create\n");
    close(listen_fd);
    return EXIT_FAILURE;
  }

  struct epoll_event listen_ev;
  memset(&listen_ev, 0, sizeof(listen_ev));
  listen_ev.events = EPOLLIN;
  listen_ev.data.fd = listen_fd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &listen_ev) < 0) {
    fprintf(stderr, "Error: epoll ctl add listen\n");
    close(listen_fd);
    return EXIT_FAILURE;
  }

  struct epoll_event evs[N_CLIENT];
  while (1) {
    // Wait epoll listener
    int n_fds = epoll_wait(epfd, evs, N_CLIENT, -1);

    // Error epoll
    if (n_fds < 0) {
      fprintf(stderr, "Error: epoll wait\n");
      close(listen_fd);
      return EXIT_FAILURE;
    }

    for (int i = 0; i < n_fds; i++) {
      if (evs[i].data.fd == listen_fd) {  // Add epoll listener
        struct sockaddr_in client_addr;
        socklen_t len_client = sizeof(client_addr);

        int conn_fd;
        if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
                              &len_client)) < 0) {
          fprintf(stderr, "Error: accept\n");
          return EXIT_FAILURE;
        }

        printf("Accept socket %d (%s : %hu)\n", conn_fd,
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        struct epoll_event conn_ev;
        memset(&conn_ev, 0, sizeof(listen_ev));
        conn_ev.events = EPOLLIN;
        conn_ev.data.fd = conn_fd;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &conn_ev) < 0) {
          fprintf(stderr, "Error: epoll ctl add listen\n");
          close(listen_fd);
          return EXIT_FAILURE;
        }
      } else if (evs[i].events & EPOLLIN) {  // Read data from client
        int sock_fd = evs[i].data.fd;
        ssize_t n = read(sock_fd, buf, BUF_SIZE);

        if (n < 0) {
          fprintf(stderr, "Error: read from socket %d\n", sock_fd);
          close(sock_fd);
        } else if (n == 0) {  // connection closed by client
          printf("Close socket %d\n", sock_fd);
          struct epoll_event sock_ev;
          memset(&sock_ev, 0, sizeof(listen_ev));
          sock_ev.events = EPOLLIN;
          sock_ev.data.fd = sock_fd;
          if (epoll_ctl(epfd, EPOLL_CTL_DEL, sock_fd, &sock_ev) < 0) {
            fprintf(stderr, "Error: epoll ctl dell\n");
            close(listen_fd);
            return EXIT_FAILURE;
          }
          close(sock_fd);
        } else {
          printf("Read %zu bytes from socket %d\n", n, sock_fd);
          write_n(sock_fd, buf, n);
          write_n(1, buf, n);
        }
      }
    }
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}