//Name: Brayden Aubry
//Class: CMPSC 311
//Assignment: Project (Client Side)
//Date: IDK

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

// server connection details
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_MSG_LEN 1024

int sockfd; // socket file descriptor

// thread to receive messages from server
void* receive_messages(void* arg) {
    char buffer[MAX_MSG_LEN];

    while (1) {
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            printf("server disconnected.\n");
            close(sockfd);
            exit(1);
        }

        buffer[bytes] = '\0';
        printf("\n%s", buffer);  // message already includes username
        printf("you: ");
        fflush(stdout);
    }

    return NULL;
}

int main() {
    struct sockaddr_in server_addr;
    char message[MAX_MSG_LEN];
    char username[50];
    pthread_t recv_thread;

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(1);
    }

    // configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        exit(1);
    }

    printf("connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    // prompt for username and send it to server
    printf("enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';  // remove newline
    send(sockfd, username, strlen(username), 0);

    // start thread to receive messages
    pthread_create(&recv_thread, NULL, receive_messages, NULL);

    // loop to send messages
    while (1) {
        printf("you: ");
        if (fgets(message, MAX_MSG_LEN, stdin) != NULL) {
            send(sockfd, message, strlen(message), 0);
        }
    }

    return 0;
}

