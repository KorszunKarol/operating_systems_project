#include "network.h"

Connection* create_server(const char *ip, int port) {
    Connection *conn = malloc(sizeof(Connection));
    if (!conn) return NULL;

    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd < 0) {
        free(conn);
        return NULL;
    }

    int opt = 1;
    if (setsockopt(conn->fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    conn->addr.sin_addr.s_addr = inet_addr(ip);

    if (bind(conn->fd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) < 0) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    if (listen(conn->fd, 5) < 0) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    return conn;
}

int accept_connection(Connection *server) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    return accept(server->fd, (struct sockaddr*)&client_addr, &addr_len);
}

Connection* connect_to_server(const char *ip, int port) {
    Connection *conn = malloc(sizeof(Connection));
    if (!conn) return NULL;

    conn->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->fd < 0) {
        free(conn);
        return NULL;
    }

    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    conn->addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(conn->fd, (struct sockaddr*)&conn->addr, sizeof(conn->addr)) < 0) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    return conn;
}

void close_connection(Connection *conn) {
    if (conn) {
        close(conn->fd);
        free(conn);
    }
}

ssize_t send_data(Connection *conn, const void *data, size_t len) {
    ssize_t total_sent = 0;
    const char *ptr = data;

    while (total_sent < len) {
        ssize_t sent = send(conn->fd, ptr + total_sent, len - total_sent, 0);
        if (sent <= 0) {
            return sent;  // Error or connection closed
        }
        total_sent += sent;
    }
    return total_sent;
}

ssize_t receive_data(Connection *conn, void *buffer, size_t len) {
    return recv(conn->fd, buffer, len, 0);
}