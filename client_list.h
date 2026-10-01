#ifndef JALON1_CLIENT_LIST_H
#define JALON1_CLIENT_LIST_H

#include <netinet/in.h>
#include <time.h>
#include "msg_struct.h"

struct client_info {
    int fd;
    struct sockaddr_in address;
    char nickname[NICK_LEN];
    time_t connection_time;
    struct client_info *next;
};

int client_list_add(struct client_info **clients, int fd, const struct sockaddr_in *address);
void client_list_remove(struct client_info **clients, int fd);
void client_list_destroy(struct client_info **clients);
struct client_info *client_list_find_by_fd(struct client_info *clients, int fd);
struct client_info *client_list_find_by_nickname(struct client_info *clients, const char *nickname);

#endif
