# echo_server

This is a repository that implements echo server in various ways.

## build

```sh
make all
```

## run

```sh
./fork_echo
./select_echo
./poll_echo
./epoll_echo
./io_uring_echo
```

## test

```sh
make test
```

## echo servers

- [fork_echo.c](fork_echo.c)
    - [fork](https://man7.org/linux/man-pages/man2/fork.2.html)
- [select_echo.c](select_echo.c)
    - [select](https://man7.org/linux/man-pages/man2/select.2.html)
- [poll_echo.c](poll_echo.c)
    - [poll](https://man7.org/linux/man-pages/man2/poll.2.html)
- [epoll_echo.c](epoll_echo.c)
    - [epoll](https://man7.org/linux/man-pages/man7/epoll.7.html)

- [io_uring_echo.c](io_uring_echo.c)
    - [io_uring](https://kernel.dk/io_uring.pdf)([liburing](https://github.com/axboe/liburing))
