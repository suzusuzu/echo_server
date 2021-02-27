#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BACKLOG_SIZE 5
#define BUF_SIZE 1024
#define N_CLIENT 256
#define N_ENTRY 2048
#define GID 1

enum {
  ACCEPT,
  READ,
  WRITE,
  PROVIDE_BUFFER,
};

typedef struct UserData {
  __u32 fd;
  __u16 type;
  __u16 bid;
} UserData;

int listen_fd;
char bufs[N_CLIENT][BUF_SIZE] = {0};

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
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
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

  // Initialize io_uring
  struct io_uring ring;
  struct io_uring_sqe *sqe;
  struct io_uring_cqe *cqe;
  int init_ret = io_uring_queue_init(N_ENTRY, &ring, 0);
  if (init_ret < 0) {
    fprintf(stderr, "Error: init io_uring queue %d\n", init_ret);
    close(listen_fd);
    return EXIT_FAILURE;
  }

  // Setup automatic buffers
  sqe = io_uring_get_sqe(&ring);
  io_uring_prep_provide_buffers(sqe, bufs, BUF_SIZE, N_CLIENT, GID, 0);
  io_uring_submit(&ring);
  io_uring_wait_cqe(&ring, &cqe);
  if (cqe->res < 0) {
    fprintf(stderr, "Error: setup buffer %d\n", cqe->res);
    return EXIT_FAILURE;
  }
  io_uring_cqe_seen(&ring, cqe);

  // Setup first accept
  sqe = io_uring_get_sqe(&ring);
  io_uring_prep_accept(sqe, listen_fd, (struct sockaddr *)&client_addr,
                       &client_len, 0);
  io_uring_sqe_set_flags(sqe, 0);
  UserData conn_info = {
      .fd = listen_fd,
      .type = ACCEPT,
  };
  memcpy(&sqe->user_data, &conn_info, sizeof(conn_info));

  while (1) {
    io_uring_submit_and_wait(&ring, 1);
    unsigned int head;
    unsigned int cnt = 0;

    // foreach in CEQ
    io_uring_for_each_cqe(&ring, head, cqe) {
      cnt++;
      struct UserData conn_info;
      memcpy(&conn_info, &cqe->user_data, sizeof(conn_info));
      int type = conn_info.type;

      if (cqe->res == -ENOBUFS) {
        fprintf(stderr, "Error: no buffer %d\n", cqe->res);
        close(listen_fd);
        return EXIT_FAILURE;
      } else if (type == PROVIDE_BUFFER) {
        if (cqe->res < 0) {
          fprintf(stderr, "Error: provide buffer %d\n", cqe->res);
          close(listen_fd);
          return EXIT_FAILURE;
        }
      } else if (type == ACCEPT) {
        int conn_fd = cqe->res;
        if (conn_fd >= 0) {  // no error
          // Read from client
          sqe = io_uring_get_sqe(&ring);
          io_uring_prep_recv(sqe, conn_fd, NULL, BUF_SIZE, 0);
          io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
          sqe->buf_group = GID;

          UserData read_info = {
              .fd = conn_fd,
              .type = READ,
          };
          memcpy(&sqe->user_data, &read_info, sizeof(read_info));
        }

        // Add new client
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, listen_fd, (struct sockaddr *)&client_addr,
                             &client_len, 0);
        io_uring_sqe_set_flags(sqe, 0);
        UserData conn_info = {
            .fd = listen_fd,
            .type = ACCEPT,
        };
        memcpy(&sqe->user_data, &conn_info, sizeof(conn_info));
      } else if (type == READ) {
        int n_byte = cqe->res;
        if (cqe->res <= 0) {  // connection closed by client
          printf("Close socket %d\n", conn_info.fd);
          close(conn_info.fd);
        } else {
          // Add Write
          printf("Read %d bytes from socket %d\n", n_byte, conn_info.fd);
          int bid = cqe->flags >> IORING_CQE_BUFFER_SHIFT;
          sqe = io_uring_get_sqe(&ring);
          io_uring_prep_send(sqe, conn_info.fd, &bufs[bid], n_byte, 0);
          write_n(1, (char *)&bufs[bid], n_byte);
          io_uring_sqe_set_flags(sqe, 0);
          UserData write_info = {
              .fd = conn_info.fd,
              .type = WRITE,
              .bid = bid,
          };
          memcpy(&sqe->user_data, &write_info, sizeof(write_info));
        }
      } else if (type == WRITE) {
        // Add buffer
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_provide_buffers(sqe, bufs[conn_info.bid], BUF_SIZE, 1,
                                      GID, conn_info.bid);
        UserData provide_buffer_info = {
            .fd = 0,
            .type = PROVIDE_BUFFER,
        };
        memcpy(&sqe->user_data, &provide_buffer_info,
               sizeof(provide_buffer_info));

        // Add read
        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_recv(sqe, conn_info.fd, NULL, BUF_SIZE, 0);
        io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
        sqe->buf_group = GID;

        UserData read_info = {
            .fd = conn_info.fd,
            .type = READ,
        };
        memcpy(&sqe->user_data, &read_info, sizeof(read_info));
      }
    }

    io_uring_cq_advance(&ring, cnt);
  }

  close(listen_fd);
  return EXIT_SUCCESS;
}