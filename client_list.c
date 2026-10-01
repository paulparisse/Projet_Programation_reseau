#include "client_list.h"

#include <stdlib.h>
#include <string.h>

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address) {
    struct client_info *client = malloc(sizeof(*client));
    if (client == NULL) {
        return -1;
    }
    client->fd = fd;
    client->address = *address;
    client->nickname[0] = '\0';
    client->connection_time = time(NULL);
    client->next = *clients;
    *clients = client;
    return 0;
}

void client_list_remove(struct client_info **clients, int fd) {
    struct client_info **cursor = clients;

    while (*cursor != NULL) {
        if ((*cursor)->fd == fd) {
            struct client_info *removed = *cursor;
            *cursor = removed->next;
            free(removed);
            return;
        }
        cursor = &(*cursor)->next;
    }
}

void client_list_destroy(struct client_info **clients) {
    while (*clients != NULL) {
        struct client_info *removed = *clients;
        *clients = removed->next;
        free(removed);
    }
}

struct client_info *client_list_find_by_fd(struct client_info *clients, int fd) {
    while (clients != NULL) {
        if (clients->fd == fd) {
            return clients;
        }
        clients = clients->next;
    }
    return NULL;
}

struct client_info *client_list_find_by_nickname(struct client_info *clients, const char *nickname) {
    while (clients != NULL) {
        if (strcmp(clients->nickname, nickname) == 0) {
            return clients;
        }
        clients = clients->next;
    }
    return NULL;
}
