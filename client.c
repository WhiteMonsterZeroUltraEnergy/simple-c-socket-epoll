/**
 * @file client.c
 *
 * @brief A very simple TCP echo-client to connect to a server that is next door in the directory.
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
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

/* Flag to control the main loop when receiving SIGINT (Ctrl+C). */
volatile sig_atomic_t keep_running = 1;


/* Signal handling ensuring safe shutdown. */
void handle_sigint(int sig) {
    keep_running = 0;
}

/* It sends all data through the socket.
 * Returns 0 on success, -1 on failure, -2 if socket would block.
 */
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


int main(const int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    const char *SERVER_IP = argv[1];
    const uint16_t PORT = (uint16_t)atoi(argv[2]);

    /* Resolve the hostname to an IP address. */
    const struct hostent *server = gethostbyname(SERVER_IP);
    if (server == NULL) {
        perror("gethostbyname");
        exit(2);
    }

    /* Create a TCP socket. */
    const int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket");
        exit(3);
    }

    /* Set up server address struct. */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr = *((struct in_addr *) server->h_addr);
    memset(&(server_addr.sin_zero), '\0', 8);

    /* Attempt to connect to the server. */
    if (connect(client_fd, (struct sockaddr *)&server_addr,sizeof(struct sockaddr)) == -1) {
        perror("connect");
        exit(4);
    }

    fprintf(stderr, "[*] [%s:%d] Connected to server.\n", SERVER_IP, PORT);
    fprintf(stdout, "Type \"exit\" to end the connection.\n");
    /* Main loop.*/
    while (keep_running) {
        fprintf(stdout, "> ");
        if (!fgets(buffer, BUFFER_SIZE, stdin)) break;
        if (strncmp(buffer, "exit", 4) == 0) break;

        /* Send user input to server. */
        if (send_all(client_fd, buffer, strlen(buffer)) < 0) {
            perror("send_all");
            continue;
        }

        /* Receive response from server. */
        const ssize_t bytes_received = read(client_fd, buffer, BUFFER_SIZE);
        if (bytes_received == -1) {
            perror("recv");
        }else if (bytes_received == 0) {
            break;
        }
        else {
            buffer[bytes_received] = '\0';
            fprintf(stdout, "%s", buffer);
        }
    }

    /* Shutdown and close the socket. */
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    fprintf(stderr, "[*] Connection closed.\n");
    return 0;
}
