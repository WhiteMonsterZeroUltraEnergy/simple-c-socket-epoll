/**
 * @file server.c
 *
 * @brief Simple TCP echo server using epoll and non-blocking I/O.
 *
 * @author WhiteMonsterZeroUltraEnergy
 * @license MIT
 *
 * https://github.com/WhiteMonsterZeroUltraEnergy
 *
 * @copyright
 * MIT License
 *
 * Copyright (c) 2025 WhiteMonsterZeroUltraEnergy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define PORT 3490
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10


/* Flag to control the main loop when receiving SIGINT (Ctrl+C). */
volatile sig_atomic_t keep_running = 1;


/* Signal handling ensuring safe shutdown. */
void handle_sigint(int sig) {
    keep_running = 0;
}


/* Sets the file descriptor to non-blocking mode. */
int set_nonblock(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


/* It sends all data through the socket. */
int send_all(const int socket, const void *msg, const size_t len) {
    const char *data = msg;
    size_t total_sent = 0;

    while (total_sent < len) {
        const ssize_t bytes_sent = write(socket, data + total_sent, len - total_sent);
        if (bytes_sent < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return -2;
            return -1;
        }
        /* socket is closed */
        if (bytes_sent == 0) {
            break;
        }
        total_sent += (size_t) bytes_sent;
    }
    return (total_sent == len) ? 0 : -1;
}

/* Closes the connection with socket */
void close_socket(const int fd, const int epoll_fd) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int main(void) {
    struct sockaddr_in server_addr, peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    char buffer[BUFFER_SIZE];
    struct epoll_event epoll_event;
    struct epoll_event epoll_events_queue[MAX_EVENTS];

    /* no buffering for printf */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* Creating a listening socket. */
    const int LISTEN_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (LISTEN_FD == -1) {
        perror("socket");
        exit(1);
    }

    /* Allow reuse of address. */
    if (setsockopt(LISTEN_FD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(3);
    }

    /* Bind to specified port. */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(LISTEN_FD, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        exit(5);
    }

    /* Start listening. */
    if (listen(LISTEN_FD, SOMAXCONN) == -1) {
        perror("listen");
        exit(6);
    }

    /* Set listening socket to non-blocking. */
    if (set_nonblock(LISTEN_FD) == -1) {
        perror("set_nonblock");
        exit(7);
    }

    /* Create an epoll instance. */
    const int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(8);
    }

    /* Register listening socket to epoll. */
    epoll_event.events = EPOLLIN;
    epoll_event.data.fd = LISTEN_FD;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, LISTEN_FD, &epoll_event) == -1) {
        perror("epoll_ctl");
        exit(9);
    }

    /* Set up signal handling. */
    signal(SIGINT, handle_sigint);

    /* Main loop */
    fprintf(stderr,"[*] Server is running.\n");
    while (keep_running) {
        const int fds_ready = epoll_wait(epoll_fd, epoll_events_queue, MAX_EVENTS, 1000);
        if (fds_ready == -1) {
            /* Interrupted by a signal. */
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < fds_ready; ++i) {
            if (epoll_events_queue[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                /* Client closed connection. */
                close_socket(epoll_events_queue[i].data.fd, epoll_fd);
                fprintf(stdout, "[-] Peer disconnected from server.\n");
                continue;
            }
            /* New incoming connection. */
            if (epoll_events_queue[i].data.fd == LISTEN_FD) {
                const int peer_fd = accept(LISTEN_FD, (struct sockaddr *) &peer_addr, &addr_len);
                if (peer_fd == -1) {
                    perror("accept");
                    continue;
                }

                set_nonblock(peer_fd);
                epoll_event.data.fd = peer_fd;
                epoll_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, peer_fd, &epoll_event)) {
                    perror("epoll_ctl");
                }
                fprintf(stdout, "[*] New Connection\n");
            }
            else {
                while (true) {
                    const ssize_t bytes_received = read(epoll_events_queue[i].data.fd, buffer, BUFFER_SIZE);
                    if (bytes_received > 0) {
                        /* Received a few bytes */
                        if (send_all(epoll_events_queue[i].data.fd, buffer, bytes_received)) {
                            perror("send_all");
                        }
                        fprintf(stdout, "[*] Received: %ld bytes\n", bytes_received);
                    }
                    else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        /* No more data to read. */
                        break;
                    }else if (bytes_received == 0) {
                        /* Client closed connection. */
                        close_socket(epoll_events_queue[i].data.fd, epoll_fd);
                        fprintf(stdout, "[-] Peer disconnected from server.\n");
                        break;
                    }else {
                        perror("read");
                    }
                }
            }
        }
    }

    close(epoll_fd);
    close(LISTEN_FD);

    fprintf(stderr,"[*] Server closed.\n");
    return 0;
}
