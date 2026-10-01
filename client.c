#include "common.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096

int setup_connection(const char *server_ip, const char *server_port) {
    int socket_fd;
    int result;
    struct sockaddr_in server_address;

    printf("Using server IPv4 address %s.\n", server_ip);
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    result = inet_aton(server_ip, &server_address.sin_addr);
    if (result == 0) {
        fprintf(stderr, "Invalid IPv4 address: %s\n", server_ip);
        return -1;
    }

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    die(socket_fd, "socket");
    printf("TCP socket created.\n");

    server_address.sin_port = htons((unsigned short)atoi(server_port));
    result = connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    die(result, "connect");
    printf("Connected to %s:%s.\n", inet_ntoa(server_address.sin_addr), server_port);
    return socket_fd;
}

/* Envoie une structure puis son payload éventuel. */
int send_message(int socket_fd, struct message *msg, char *payload) {
    if (write_in_socket(socket_fd, msg, sizeof(*msg)) == 0) {
        return 0;
    }
    if (msg->pld_len > 0 && write_in_socket(socket_fd, payload, msg->pld_len) == 0) {
        return 0;
    }
    return 1;
}

/* Lit un message venant du serveur. */
int read_server_message(int socket_fd) {
    struct message msg;
    char payload[MAX_MESSAGE_SIZE + 1];

    if (read_from_socket(socket_fd, &msg, sizeof(msg)) == 0) {
        return 0;
    }
    if (msg.pld_len < 0 || msg.pld_len > MAX_MESSAGE_SIZE) {
        fprintf(stderr, "Invalid payload size from server: %d\n", msg.pld_len);
        return 0;
    }

    if (msg.pld_len > 0) {
        if (read_from_socket(socket_fd, payload, msg.pld_len) == 0) {
            return 0;
        }
        payload[msg.pld_len] = '\0';

        if (msg.nick_sender[0] != '\0' && strcmp(msg.nick_sender, "Server") != 0 &&
            (msg.type == UNICAST_SEND || msg.type == BROADCAST_SEND)) {
            printf("[%s] : %s\n", msg.nick_sender, payload);
        } else {
            printf("%s\n", payload);
        }
    }
    return 1;
}

/* Transforme la ligne tapée en message du protocole. */
int get_and_send_user_message(int socket_fd) {
    char input[MAX_MESSAGE_SIZE + 1];
    char *payload = NULL;
    ssize_t bytes_read;
    struct message msg;

    bytes_read = read(STDIN_FILENO, input, MAX_MESSAGE_SIZE);
    die((int)bytes_read, "read stdin");
    if (bytes_read == 0) {
        return 0;
    }

    input[bytes_read] = '\0';
    input[strcspn(input, "\n")] = '\0';

    if (strcmp(input, "/quit") == 0) {
        return 0;
    }

    memset(&msg, 0, sizeof(msg));

    if (strncmp(input, "/nick ", 6) == 0) {
        msg.type = NICKNAME_NEW;
        strncpy(msg.infos, input + 6, INFOS_LEN - 1);
    } else if (strcmp(input, "/who") == 0) {
        msg.type = NICKNAME_LIST;
    } else if (strncmp(input, "/whois ", 7) == 0) {
        msg.type = NICKNAME_INFOS;
        strncpy(msg.infos, input + 7, INFOS_LEN - 1);
    } else if (strncmp(input, "/msgall ", 8) == 0) {
        msg.type = BROADCAST_SEND;
        payload = input + 8;
        msg.pld_len = (int)strlen(payload);
    } else if (strncmp(input, "/msg ", 5) == 0) {
        char *space = strchr(input + 5, ' ');
        if (space == NULL) {
            printf("Usage: /msg <pseudo> <message>\n");
            return 1;
        }
        *space = '\0';
        msg.type = UNICAST_SEND;
        strncpy(msg.infos, input + 5, INFOS_LEN - 1);
        payload = space + 1;
        msg.pld_len = (int)strlen(payload);
    } else {
        msg.type = ECHO_SEND;
        payload = input;
        msg.pld_len = (int)strlen(payload);
    }

    return send_message(socket_fd, &msg, payload);
}

void client_poll_loop(int socket_fd) {
    struct pollfd watched[2];
    int running = 1;

    /* Même poll que dans le jalon 1. */
    watched[0].fd = STDIN_FILENO;
    watched[0].events = POLLIN;
    watched[1].fd = socket_fd;
    watched[1].events = POLLIN;

    while (running) {
        int ready = poll(watched, 2, -1);
        die(ready, "poll");

        if ((watched[1].revents & POLLIN) != 0) {
            running = read_server_message(socket_fd);
        }

        if (running && (watched[0].revents & POLLIN) != 0) {
            running = get_and_send_user_message(socket_fd);
        }

        if ((watched[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
            (watched[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            running = 0;
        }
    }
}

int main(int argc, char **argv) {
    int socket_fd;

    if (argc != 3) {
        fprintf(stderr, "Usage: ./client <server_ipv4> <server_port>\n");
        return EXIT_FAILURE;
    }
    socket_fd = setup_connection(argv[1], argv[2]);
    if (socket_fd < 0) {
        return EXIT_FAILURE;
    }
    client_poll_loop(socket_fd);
    close(socket_fd);
    return EXIT_SUCCESS;
}
