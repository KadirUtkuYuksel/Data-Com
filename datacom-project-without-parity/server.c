#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdint.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define USERNAME_LEN 32
#define MESSAGE_LEN (BUFFER_SIZE - USERNAME_LEN - 10)

int PORT = 0; // Portu 0 olarak ayarlıyoruz, dinamik port bulunacak

// Client structure
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char username[USERNAME_LEN];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int uid = 10;

// Add client to the list
void add_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (!clients[i])
        {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Remove client from the list
void remove_client(int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Broadcast message to all clients
void broadcast_message(char *message, int uid)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (clients[i]->uid != uid)
            {
                if (write(clients[i]->sockfd, message, strlen(message)) < 0)
                {
                    perror("Error sending message");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send private message
void send_private_message(char *target_username, char *message)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i)
    {
        if (clients[i])
        {
            if (strcmp(clients[i]->username, target_username) == 0)
            {
                if (write(clients[i]->sockfd, message, strlen(message)) < 0)
                {
                    perror("Error sending private message");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Handle client communication
void *handle_client(void *arg)
{
    char buffer[MESSAGE_LEN];
    char formatted_message[BUFFER_SIZE];
    int leave_flag = 0;
    client_t *cli = (client_t *)arg;

    printf("%s joined\n", cli->username);

    while (1)
    {
        if (leave_flag)
        {
            break;
        }

        int receive = recv(cli->sockfd, buffer, MESSAGE_LEN, 0);
        if (receive > 0)
        {
            buffer[receive] = '\0';

            // Özel mesaj
            if (buffer[0] == '@')
            {
                char *target_user = strtok(buffer + 1, " ");
                char *message = strtok(NULL, "");

                if (target_user && message)
                {
                    snprintf(formatted_message, sizeof(formatted_message), "[Private] %s: %.900s\n", cli->username, message);
                    send_private_message(target_user, formatted_message);
                }
            }
            else
            {
                snprintf(formatted_message, sizeof(formatted_message), "%s: %.900s\n", cli->username, buffer);
                broadcast_message(formatted_message, cli->uid);
            }
        }
        else if (receive == 0 || strcmp(buffer, "exit") == 0)
        {
            printf("%s left\n", cli->username);
            leave_flag = 1;
        }
        else
        {
            perror("Receive error");
            leave_flag = 1;
        }

        bzero(buffer, MESSAGE_LEN);
        bzero(formatted_message, BUFFER_SIZE);
    }

    close(cli->sockfd);
    remove_client(cli->uid);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}

int main()
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;
    socklen_t len = sizeof(serv_addr);

    // Socket setup
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT); // Portu 0 olarak ayarladık

    // Bind the socket
    if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Bind failed");
        return EXIT_FAILURE;
    }

    // Dinamik olarak işletim sistemi tarafından atanan portu almak
    if (getsockname(listenfd, (struct sockaddr *)&serv_addr, &len) == -1)
    {
        perror("getsockname");
        return EXIT_FAILURE;
    }

    PORT = ntohs(serv_addr.sin_port); // Atanan portu alıyoruz ve ekranda gösteriyoruz
    printf("Server started on port %d...\n", PORT);

    // Listen for connections
    if (listen(listenfd, 10) < 0)
    {
        perror("Listen failed");
        return EXIT_FAILURE;
    }

    while (1)
    {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr *)&cli_addr, &clilen);

        if ((connfd < 0))
        {
            perror("Accept failed");
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        // Get the username
        recv(cli->sockfd, cli->username, USERNAME_LEN, 0);
        cli->username[strcspn(cli->username, "\n")] = '\0';

        add_client(cli);
        pthread_create(&tid, NULL, &handle_client, (void *)cli);
    }

    return EXIT_SUCCESS;
}