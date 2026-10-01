#include "common.h"
#include "client_list.h"
#include "msg_struct.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAX_MESSAGE_SIZE 4096
#define MAX_CLIENTS 128

int setup_listening_socket(int port) {
    int listen_fd;
    int result;
    struct sockaddr_in server_address;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    die(listen_fd, "socket");
    printf("TCP listening socket created.\n");

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons((unsigned short)port);
    result = bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    die(result, "bind");
    printf("Socket bound to port %d.\n", port);

    result = listen(listen_fd, 20);
    die(result, "listen");
    printf("Listening for client connections.\n");
    return listen_fd;
}

void accept_and_insert_client(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_address_length);
    int slot;

    die(client_fd, "accept");
    for (slot = 1; slot < MAX_CLIENTS; slot++) {
        if (poll_fds[slot].fd < 0) {
            if (client_list_add(clients, client_fd, &client_address) < 0) {
                close(client_fd);
                die(-1, "malloc client information");
            }
            poll_fds[slot].fd = client_fd;
            poll_fds[slot].events = POLLIN;
            poll_fds[slot].revents = 0;
            printf("Accepted client %s:%u on slot %d.\n",
                   inet_ntoa(client_address.sin_addr),
                   (unsigned int)ntohs(client_address.sin_port), slot);
            break;
        }
    }
    if (slot == MAX_CLIENTS) {
        fprintf(stderr, "Client limit reached. Closing the new connection.\n");
        close(client_fd);
    }
}

/* Envoie une structure puis son payload éventuel. */
int send_message(int fd, enum msg_type type, const char *sender, const char *infos, const char *payload) {
    struct message msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = type;

    if (sender != NULL) {
        strncpy(msg.nick_sender, sender, NICK_LEN - 1);
    }
    if (infos != NULL) {
        strncpy(msg.infos, infos, INFOS_LEN - 1);
    }
    if (payload != NULL) {
        msg.pld_len = (int)strlen(payload);
    }

    if (write_in_socket(fd, &msg, sizeof(msg)) == 0) {
        return 0;
    }
    if (msg.pld_len > 0 && write_in_socket(fd, (void *)payload, msg.pld_len) == 0) {
        return 0;
    }
    return 1;
}

int valid_nickname(const char *nickname) {
    size_t i;

    if (nickname[0] == '\0' || strlen(nickname) >= NICK_LEN) {
        return 0;
    }
    for (i = 0; nickname[i] != '\0'; i++) {
        if (!isalnum((unsigned char)nickname[i])) {
            return 0;
        }
    }
    return 1;
}

/* Traite un message reçu d'un client. */
int handle_client_message(int client_fd, struct client_info *clients) {
    struct message msg;
    struct client_info *sender;
    char payload[MAX_MESSAGE_SIZE + 1];

    if (read_from_socket(client_fd, &msg, sizeof(msg)) == 0) {
        fprintf(stderr, "Client %d : Socket close\n", client_fd);
        return 1;
    }
    if (msg.pld_len < 0 || msg.pld_len > MAX_MESSAGE_SIZE) {
        fprintf(stderr, "Client %d : Error on payload size (%d)\n", client_fd, msg.pld_len);
        return 1;
    }

    if (msg.pld_len > 0) {
        if (read_from_socket(client_fd, payload, msg.pld_len) == 0) {
            return 1;
        }
        payload[msg.pld_len] = '\0';
    } else {
        payload[0] = '\0';
    }

    sender = client_list_find_by_fd(clients, client_fd);
    if (sender == NULL) {
        return 1;
    }

    if (msg.type == NICKNAME_NEW) {
        struct client_info *other;
        char response[256];

        if (!valid_nickname(msg.infos)) {
            send_message(client_fd, NICKNAME_NEW, "Server", "", "Invalid nickname");
            return 0;
        }

        other = client_list_find_by_nickname(clients, msg.infos);
        if (other != NULL && other->fd != client_fd) {
            send_message(client_fd, NICKNAME_NEW, "Server", "", "Nickname already used");
            return 0;
        }

        strncpy(sender->nickname, msg.infos, NICK_LEN - 1);
        sender->nickname[NICK_LEN - 1] = '\0';
        snprintf(response, sizeof(response), "Welcome on the chat %s", sender->nickname);
        send_message(client_fd, NICKNAME_NEW, "Server", sender->nickname, response);
    }

    else if (msg.type == NICKNAME_LIST) {
        struct client_info *current = clients;
        char response[MAX_MESSAGE_SIZE + 1] = "Online users are\n";
        size_t used = strlen(response);

        while (current != NULL) {
            if (current->nickname[0] != '\0') {
                int n = snprintf(response + used, sizeof(response) - used, " - %s\n", current->nickname);
                if (n < 0 || (size_t)n >= sizeof(response) - used) {
                    break;
                }
                used += (size_t)n;
            }
            current = current->next;
        }
        send_message(client_fd, NICKNAME_LIST, "Server", "", response);
    }

    else if (msg.type == NICKNAME_INFOS) {
        struct client_info *target = client_list_find_by_nickname(clients, msg.infos);
        char response[512];
        char date[64];
        char ip[INET_ADDRSTRLEN];
        struct tm *tm_info;

        if (target == NULL) {
            send_message(client_fd, NICKNAME_INFOS, "Server", "", "User not found");
            return 0;
        }

        inet_ntop(AF_INET, &target->address.sin_addr, ip, sizeof(ip));
        tm_info = localtime(&target->connection_time);
        strftime(date, sizeof(date), "%Y/%m/%d@%H:%M", tm_info);
        snprintf(response, sizeof(response),
                 "%s connected since %s with IP address %s and port number %u",
                 target->nickname, date, ip, (unsigned int)ntohs(target->address.sin_port));
        send_message(client_fd, NICKNAME_INFOS, "Server", "", response);
    }

    else if (msg.type == BROADCAST_SEND) {
        struct client_info *current = clients;

        while (current != NULL) {
            if (current->fd != client_fd && current->nickname[0] != '\0') {
                send_message(current->fd, BROADCAST_SEND, sender->nickname, "", payload);
            }
            current = current->next;
        }
    }

    else if (msg.type == UNICAST_SEND) {
        struct client_info *target = client_list_find_by_nickname(clients, msg.infos);

        if (target == NULL) {
            send_message(client_fd, UNICAST_SEND, "Server", "", "User not found");
        } else {
            send_message(target->fd, UNICAST_SEND, sender->nickname, "", payload);
        }
    }

    else if (msg.type == ECHO_SEND) {
        send_message(client_fd, ECHO_SEND, sender->nickname, "", payload);
    }

    return 0;
}

void server_poll_loop(int listen_fd, struct pollfd poll_fds[MAX_CLIENTS], struct client_info **clients) {
    int running = 1;

    /* Même tableau poll que dans le jalon 1. */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        poll_fds[i].fd = -1;
        poll_fds[i].events = 0;
        poll_fds[i].revents = 0;
    }
    poll_fds[0].fd = listen_fd;
    poll_fds[0].events = POLLIN;

    while (running) {
        int ready = poll(poll_fds, MAX_CLIENTS, -1);
        die(ready, "poll");

        if ((poll_fds[0].revents & POLLIN) != 0) {
            accept_and_insert_client(listen_fd, poll_fds, clients);
        }

        for (int slot = 1; slot < MAX_CLIENTS; slot++) {
            short returned_events = poll_fds[slot].revents;
            int close_connection = 0;
            if (poll_fds[slot].fd < 0) {
                continue;
            }

            if ((returned_events & POLLIN) != 0) {
                close_connection = handle_client_message(poll_fds[slot].fd, *clients);
            }
            if ((returned_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                close_connection = 1;
            }
            if (close_connection) {
                int client_fd = poll_fds[slot].fd;
                close(client_fd);
                client_list_remove(clients, client_fd);
                poll_fds[slot].fd = -1;
                poll_fds[slot].events = 0;
                poll_fds[slot].revents = 0;
            }
        }
        if ((poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            running = 0;
        }
    }

    for (int slot = 1; slot < MAX_CLIENTS; slot++) {
        if (poll_fds[slot].fd >= 0) {
            close(poll_fds[slot].fd);
            poll_fds[slot].fd = -1;
        }
    }
    client_list_destroy(clients);
}

int main(int argc, char **argv) {
    struct pollfd poll_fds[MAX_CLIENTS];
    struct client_info *clients = NULL;
    int port;
    int listen_fd;

    if (argc != 2) {
        fprintf(stderr, "Usage: ./server <server_port>\n");
        return EXIT_FAILURE;
    }
    port = atoi(argv[1]);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return EXIT_FAILURE;
    }

    listen_fd = setup_listening_socket(port);
    server_poll_loop(listen_fd, poll_fds, &clients);
    close(listen_fd);
    return EXIT_SUCCESS;
}
