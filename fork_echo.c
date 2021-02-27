#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG_SIZE 5
#define BUF_SIZE 1024
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

  while (1) {
    // Check new connection
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

    pid_t pid = fork();

    if (pid < 0) {
      fprintf(stderr, "Error: fork\n");
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      // child
      char buf[BUF_SIZE];
      close(listen_fd);
      while (1) {
        ssize_t n = read(conn_fd, buf, BUF_SIZE);

        if (n < 0) {
          fprintf(stderr, "Error: read from socket %d\n", conn_fd);
          close(conn_fd);
          exit(-1);
        } else if (n == 0) {  // connection closed by client
          printf("Close socket %d\n", conn_fd);
          close(conn_fd);
          exit(0);
        } else {
          printf("Read %zu bytes from socket %d\n", n, conn_fd);
          write_n(conn_fd, buf, n);
        }
      }
    } else {
      // parent
      close(conn_fd);
    }
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}