#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <sys/socket.h>
#include <ctype.h>

// server connection details
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_MSG_LEN 1024

// application details
#define APP_NAME "Chat App"

int sockfd; // socket file descriptor
char username_global[50]; //username variable to store username entry from GUI
GtkTextBuffer *global_chat_buffer; // chat box components
GtkTextView *global_chat_view;
GtkWidget *msg_entry;
GtkWidget *stack; // page stack
pthread_t recv_thread; // the receive messages thread
bool recv_running = false;


// appends the message (data) to the chat buffer. this is only called from the gtk main loop after being attached in receive_messages()
static gboolean append_message_to_buffer(gpointer data)
{
    GtkTextIter iter;
    char *message = data;

    // append message to buffer
    gtk_text_buffer_get_end_iter(global_chat_buffer, &iter);
    gtk_text_buffer_insert(global_chat_buffer, &iter, message, -1);
    gtk_text_buffer_insert(global_chat_buffer, &iter, "\n", 1);

    // scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_create_mark(global_chat_buffer, NULL, &iter, false);
    gtk_text_view_scroll_to_mark(global_chat_view, mark, 0, false, 0, 1);

    g_free(message);
    return G_SOURCE_REMOVE; // remove function from gtk main loop

}

// go back to the login page and clean up
static gboolean show_login_page(gpointer data)
{
    if (recv_running)
    {
        recv_running = false;
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
        pthread_join(recv_thread, NULL);
    }

    // clear chat history and message entry
    gtk_text_buffer_set_text(global_chat_buffer, "", -1);
    gtk_editable_set_text(GTK_EDITABLE(msg_entry), "");

    // go back to login page
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "login");

    return G_SOURCE_REMOVE;
}

// thread to receive messages from server
void* receive_messages(void* arg) {
    char buffer[MAX_MSG_LEN];

    recv_running = true;
    while (recv_running) {
        int bytes = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            printf("server disconnected.\n");
            recv_running = false;
            g_idle_add(show_login_page, NULL);
        }

        buffer[bytes] = '\0';
        // this appends the message to the chat buffer. it has to go through gtk's thread or it kicks and screams
        g_idle_add(append_message_to_buffer, g_strdup(buffer));
    }

    recv_running = false;
    return NULL;
}

// checks whether the provided username is valid or not. a username is valid if it is not empty and is only alphanumeric characters
static bool is_valid_username(const char *username)
{
    if (username == NULL || username[0] == '\0') // return false if string is empty
    {
        return false;
    }
    for (size_t i = 0; i < strlen(username); i++)
    {
        if(!isalnum(username[i]))
        {
            return false;
        }
    }
    return true;
}

//function to retrieve the username entry from the text box when the login_button is clicked
static void on_login_button_clicked(GtkButton *button, gpointer data) {
    // retrieve data from button
    GtkEntry *entry = g_object_get_data(G_OBJECT(button), "entry");
    GtkStack *stack = g_object_get_data(G_OBJECT(button), "stack");
    GtkWidget *error_label = g_object_get_data(G_OBJECT(button), "error_label");

    const char *username = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (!is_valid_username(username)) // if username is invalid, show error label
    {
        gtk_widget_set_opacity(error_label, 1);
        return;
    }

    //assigns username variable in backend
    strcpy(username_global, username);

    //test to verify the username from the text entry in the GUI is assigned to the username variable in backend
    printf("Username (Global): %s\n", username); //prints to console

    // server connection

    // create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // connect to server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connection failed");
        return;
    }
    printf("connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);

    // send username to server
    // username[strcspn(username, "\n")] = '\0';  // remove newline
    send(sockfd, username_global, strlen(username_global), 0);

    // start thread to receive messages
    pthread_create(&recv_thread, NULL, receive_messages, NULL);

    // go to chat room
    gtk_widget_set_opacity(error_label, 0);
    gtk_stack_set_visible_child_name(stack, "chat room");
}

static void on_message_sent(GtkEntry *entry, gpointer data)
{
    const char *text = gtk_editable_get_text((GTK_EDITABLE(entry)));
    GtkTextIter iter;
    GtkStack *stack = GTK_STACK(data);

    if (*text == '\0')
    {
        return; // ignore empty inputs
    }

    // send message to server
    if (send(sockfd, text, strlen(text), 0) <= 0)
    {
        g_idle_add(show_login_page, NULL);
        return;
    }

    // append the new message onto the buffer
    gtk_text_buffer_get_end_iter(global_chat_buffer, &iter);
    gtk_text_buffer_insert(global_chat_buffer, &iter, username_global, -1);
    gtk_text_buffer_insert(global_chat_buffer, &iter, ": ", 2);
    gtk_text_buffer_insert(global_chat_buffer, &iter, text, -1);
    gtk_text_buffer_insert(global_chat_buffer, &iter, "\n\n", 2);

    // scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_create_mark(global_chat_buffer, NULL, &iter, false);
    gtk_text_view_scroll_to_mark(global_chat_view, mark, 0, false, 0, 1);

    // clear the input box text for next message
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
}

static void on_leave_button_clicked(GtkButton *button, gpointer data)
{
    g_idle_add(show_login_page, NULL);
}

// sets up the app when its activated (started)
static void on_activate(GtkApplication *app, gpointer user_data)
{
    // set up window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 800);
    gtk_window_set_title(GTK_WINDOW(window), APP_NAME);
    gtk_window_present(GTK_WINDOW(window));

    // create page stack
    stack = gtk_stack_new();
    gtk_widget_set_hexpand(stack, TRUE); // expand the stack to the whole window
    gtk_widget_set_vexpand(stack, TRUE);
    gtk_widget_set_margin_start(stack, 15); // add padding around the edges
    gtk_widget_set_margin_end(stack, 15);
    gtk_widget_set_margin_top(stack, 15);
    gtk_widget_set_margin_bottom(stack, 15);
    gtk_window_set_child(GTK_WINDOW(window), stack); // the stack should always be the window's only child, don't add anything else

    // build login page
    GtkWidget *login_box;
    GtkWidget *login_button;
    GtkWidget *entry; //username entry
    GtkWidget *error_label;

    login_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); // button allignment
    gtk_widget_set_halign(login_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(login_box, GTK_ALIGN_CENTER);
    entry = gtk_entry_new();
    login_button = gtk_button_new_with_label("Login");
    error_label = gtk_label_new("Username must contain only alphanumeric characters.");
    gtk_widget_set_opacity(error_label, 0);// initially hide the error label, only showing it if the user enters an invalid username on login

        // to get multiple values into one connected funtion, the entry and stack and error label are set as data in login button, then retrieved in its connected function
    g_object_set_data(G_OBJECT(login_button), "entry", entry);
    g_object_set_data(G_OBJECT(login_button), "stack", stack);
    g_object_set_data(G_OBJECT(login_button), "error_label", error_label);
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_button_clicked), NULL); //calls on-login_button_clicked() when the button is clicked

    gtk_box_append(GTK_BOX(login_box), entry);
    gtk_box_append(GTK_BOX(login_box), login_button);
    gtk_box_append(GTK_BOX(login_box), error_label);


    // build chat room page
    GtkWidget *chat_room_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_hexpand(chat_room_box, TRUE);
    gtk_widget_set_vexpand(chat_room_box, TRUE);
    gtk_widget_set_halign(chat_room_box, GTK_ALIGN_FILL);
    gtk_widget_set_valign(chat_room_box, GTK_ALIGN_FILL);

        // message print area
    GtkWidget *scroller  = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scroller, TRUE);
    gtk_widget_set_vexpand(scroller, TRUE);
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroller), text_view);
    global_chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    global_chat_view = GTK_TEXT_VIEW(text_view);

        // bottom row (message input and leave button)
    GtkWidget *input_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_hexpand(input_row, TRUE);
    msg_entry = gtk_entry_new();
    gtk_widget_set_hexpand(msg_entry, TRUE);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    g_object_set_data(G_OBJECT(msg_entry), "buffer", buffer);
    g_object_set_data(G_OBJECT(msg_entry), "text_view", text_view);
    GtkWidget *leave_button = gtk_button_new_with_label("Leave");
    g_object_set_data(G_OBJECT(leave_button), "msg_entry", msg_entry);

    g_signal_connect(msg_entry, "activate", G_CALLBACK(on_message_sent), stack);
    g_signal_connect(leave_button, "clicked", G_CALLBACK(show_login_page), NULL);

    gtk_box_append(GTK_BOX(input_row), msg_entry);
    gtk_box_append(GTK_BOX(input_row), leave_button);
    gtk_box_append(GTK_BOX(chat_room_box), scroller);
    gtk_box_append(GTK_BOX(chat_room_box), input_row);

    // add pages to stack
    gtk_stack_add_named(GTK_STACK(stack), login_box, "login");
    gtk_stack_add_named(GTK_STACK(stack), chat_room_box, "chat room");



    // show login page
    gtk_stack_set_visible_child_name(GTK_STACK(stack), "login");
    gtk_window_present(GTK_WINDOW(window));
}

int main() {
    // set up app window
    GtkApplication *app;
    //g_signal_conenct_(app, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL); // app is set up in the on_activate function
    g_application_run(G_APPLICATION(app), 0, NULL);
    g_object_unref(app);

    return 0;
}
