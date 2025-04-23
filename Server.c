/*
File: Server Class
Date: 04/15/2025
*/

/*
File: Server
Date: 04/15/2025
*/

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>  // for WinMain types
    typedef int socklen_t;
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    #include <errno.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    typedef int SOCKET;
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    SOCKET socket;
    struct sockaddr_in address;
    pthread_t thread;
    char username[50];
} Client;

Client *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message, SOCKET sender_socket);
void remove_client(SOCKET socket);
void *handle_client(void *arg);
void handle_new_client(SOCKET client_socket, struct sockaddr_in client_addr);

void handle_new_client(SOCKET client_socket, struct sockaddr_in client_addr) {
    char username[50];
    recv(client_socket, username, sizeof(username), 0);
    username[strcspn(username, "\n")] = 0;

    Client *new_client = (Client *)malloc(sizeof(Client));
    if (!new_client) {
        perror("failed to allocate memory");
        return;
    }

    new_client->socket = client_socket;
    new_client->address = client_addr;
    strcpy(new_client->username, username);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) {
            clients[i] = new_client;
            pthread_create(&new_client->thread, NULL, handle_client, (void *)new_client);
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    send(client_socket, "server full. try again later.\n", 32, 0);

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif

    free(new_client);
}

void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(client->socket, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s: %s", client->username, buffer);
        broadcast_message(buffer, client->socket);
    }

    remove_client(client->socket);
    free(client);
    pthread_exit(NULL);
    return NULL; // to avoid compiler warning
}

void broadcast_message(char *message, SOCKET sender_socket) {
    pthread_mutex_lock(&clients_mutex);

    // find sender's username
    char sender_name[50] = "unknown";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == sender_socket) {
            strcpy(sender_name, clients[i]->username);
            break;
        }
    }

    // format message with username
    char full_message[BUFFER_SIZE + 100];
    snprintf(full_message, sizeof(full_message), "%s: %s", sender_name, message);

    // send to all clients except the sender
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            send(clients[i]->socket, full_message, strlen(full_message), 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


void remove_client(SOCKET socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->socket == socket) {
#ifdef _WIN32
            closesocket(socket);
#else
            close(socket);
#endif
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("winsock init failed. error code: %d\n", WSAGetLastError());
        return 1;
    }
#endif

    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    printf("server started on port %d\n", PORT);

    while (1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == INVALID_SOCKET) {
            perror("connection failed");
            continue;
        }
        handle_new_client(client_socket, client_addr);
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
