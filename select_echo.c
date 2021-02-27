#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
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
  fd_set fds;
  FD_ZERO(&fds);
  int clients[N_CLIENT];
  for (int i = 0; i < N_CLIENT; i++) {
    clients[i] = DISABLE;
  }
  memset(&fds, 0, sizeof(fds));

  // Create listen socket
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Error: socket\n");
    return EXIT_FAILURE;
  }

  // Set INT signal handler
  signal(SIGINT, int_handle);

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

  fprintf(stderr, "listen on port %d\n", port);

  FD_SET(listen_fd, &fds);

  int max_fd = listen_fd;  // max fd
  int max_i = 0;           // max client into clients[] array

  while (1) {
    FD_ZERO(&fds);
    FD_SET(listen_fd, &fds);
    for (int i = 0; i < N_CLIENT; i++) {
      if (clients[i] != DISABLE) {
        FD_SET(clients[i], &fds);
      }
    }

    int res_select = select(max_fd + 1, &fds, NULL, NULL, NULL);

    if (res_select < 0) {
      fprintf(stderr, "Error: select");
      return EXIT_FAILURE;
    }

    // Check new connection
    if (FD_ISSET(listen_fd, &fds)) {
      struct sockaddr_in client_addr;
      socklen_t len_client = sizeof(client_addr);

      int connfd;
      if ((connfd = accept(listen_fd, (struct sockaddr *)&client_addr,
                           &len_client)) < 0) {
        fprintf(stderr, "Error: accept\n");
        return EXIT_FAILURE;
      }

      printf("Accept socket %d (%s : %hu)\n", connfd,
             inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

      // Save client socket into clients array
      int i;
      for (i = 0; i < N_CLIENT; i++) {
        if (clients[i] == DISABLE) {
          clients[i] = connfd;
          break;
        }
      }

      // No enough space in clients array
      if (i == N_CLIENT) {
        fprintf(stderr, "Error: too many clients\n");
        close(connfd);
      }

      if (i > max_i) {
        max_i = i;
      }
      if (connfd > max_fd) {
        max_fd = connfd;
      }
    }

    // Check all clients to read data
    for (int i = 0; i <= max_i; i++) {
      int sock_fd;
      if ((sock_fd = clients[i]) == DISABLE) {
        continue;
      }

      // If the client is readable or errors occur
      ssize_t n = read(sock_fd, buf, BUF_SIZE);
      if (n < 0) {
        fprintf(stderr, "Error: read from socket %d\n", sock_fd);
        close(sock_fd);
        clients[i] = DISABLE;
      } else if (n == 0) {  // connection closed by client
        printf("Close socket %d\n", sock_fd);
        close(sock_fd);
        clients[i] = DISABLE;
      } else {
        printf("Read %zu bytes from socket %d\n", n, sock_fd);
        write_n(sock_fd, buf, n);
        write_n(1, buf, n);
      }
    }
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}
