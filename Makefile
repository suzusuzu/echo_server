CFLAGS = -Wall -O2
CC = gcc

.PHONY: all

all: fork_echo select_echo poll_echo epoll_echo io_uring_echo

fork_echo: fork_echo.c
	$(CC) $(CFLAGS) -o fork_echo fork_echo.c

select_echo: select_echo.c
	$(CC) $(CFLAGS) -o select_echo select_echo.c

poll_echo: poll_echo.c
	$(CC) $(CFLAGS) -o poll_echo poll_echo.c

epoll_echo: epoll_echo.c
	$(CC) $(CFLAGS) -o epoll_echo epoll_echo.c

io_uring_echo: io_uring_echo.c
	$(CC) $(CFLAGS) -o io_uring_echo io_uring_echo.c -luring

test: all
	./test.sh

clean:
	rm -f fork_echo select_echo poll_echo epoll_echo io_uring_echo
