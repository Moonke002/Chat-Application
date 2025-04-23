

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <gtk/gtk.h>

// server connection details
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_MSG_LEN 1024

// application details
#define APP_NAME "Chat App"

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

// sets up the app when its activated (started)
static void on_activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 800);
    gtk_window_set_title(GTK_WINDOW(window), APP_NAME);
    gtk_window_present(GTK_WINDOW(window));

    // login screen code - GTK
    GtkWidget *button;
    GtkWidget *box;
    GtkWidget *entry;

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); // button allignment
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_window_set_child(GTK_WINDOW(window), box);

    entry = gtk_entry_new();

    button = gtk_button_new_with_label("Login");
    g_signal_connect(button, "clicked", G_CALLBACK(button_test), entry);

    gtk_box_append(GTK_BOX(box), button);
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





    // set up app window
    GtkApplication *app;

    app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL); // app is set up in the on_activate function
    g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);





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
