/*
File: Server_Combined Class
Date: 04/15/2025
*/

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
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
#include <unistd.h>
#include <signal.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    struct sockaddr_in address;
    pthread_t thread;
    char username[50];
} Client;

Client *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_socket; // global for signal handler

//Adds timestamps when the Server updates
void timestamp_log(const char *msg) {
    time_t now = time(NULL);
    char tbuf[26];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] %s\n", tbuf, msg);
}

//Sends message to all connected clients
void broadcast_message(const char *message, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->socket != sender_socket) {
            send(clients[i]->socket, message, strlen(message), MSG_NOSIGNAL);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//Removes a disconnected client from the list
void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->socket == socket) {
            char msg[100];
            snprintf(msg, sizeof(msg), "%s has left the chat.", clients[i]->username);
            timestamp_log(msg);
            close(clients[i]->socket);
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

//Handles messages from individual clients
void *handle_client(void *arg) {
    Client *cli = (Client *)arg;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE + 100];

    snprintf(message, sizeof(message), "%s has joined the chat.\n", cli->username);
    broadcast_message(message, cli->socket);
    timestamp_log(message);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(cli->socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0;
        snprintf(message, sizeof(message), "%s: %s\n", cli->username, buffer);
        broadcast_message(message, cli->socket);
    }

    remove_client(cli->socket);
    pthread_detach(pthread_self());
    return NULL;
}

//Verifies the username received from the client is not already in use
int is_username_unique(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, username) == 0) {
            return 0;
        }
    }
    return 1;
}

//Handles new client connections
void handle_new_client(int client_socket, struct sockaddr_in client_addr) {
    char username[50] = {0};
    if (recv(client_socket, username, sizeof(username), 0) <= 0) {
        close(client_socket);
        return;
    }

    username[strcspn(username, "\n")] = 0;


    pthread_mutex_lock(&clients_mutex);
    if (!is_username_unique(username)) {
        send(client_socket, "Username already taken.\n", 25, 0);
        close(client_socket);
        pthread_mutex_unlock(&clients_mutex);
        return;
    }


    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            Client *new_client = (Client *)malloc(sizeof(Client));
            new_client->socket = client_socket;
            new_client->address = client_addr;
            strncpy(new_client->username, username, sizeof(new_client->username) - 1);
            clients[i] = new_client;

            pthread_create(&new_client->thread, NULL, handle_client, (void *)new_client);

            char logmsg[100];
            snprintf(logmsg, sizeof(logmsg), "New connection from %s:%d as %s.",
                     inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), username);
            timestamp_log(logmsg);

            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }

    send(client_socket, "Server full. Try again later.\n", 31, 0);
    close(client_socket);
    pthread_mutex_unlock(&clients_mutex);
}

//safely shuts the server down when the window is disconnected
void cleanup_server(int signo) {
    timestamp_log("Server shutting down...");

    /*
    if(signo == SIGINT) {
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i]) {
                close(clients[i]->socket);
                free(clients[i]);
                clients[i] = NULL;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);*/

    close(server_socket);
    exit(0);
}

//Main function
int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (signal(SIGINT, cleanup_server) == SIG_ERR) { //Ctrl+c handler
        fprintf(stderr, "Error with SIGINT\n");
        exit(-1);
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return EXIT_FAILURE;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Binding failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listening failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    timestamp_log("Server started.");

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        handle_new_client(client_socket, client_addr);
    }

    return 0;
}
