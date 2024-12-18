#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define BUFFER_SIZE 1024

void *receive_message(void *sockfd) {
    int sock = *((int *)sockfd);
    char message[BUFFER_SIZE];

    while (1) {
        int receive = recv(sock, message, BUFFER_SIZE, 0);
        if (receive > 0) {
            printf("%s", message);
        } else if (receive == 0) {
            break;
        }
        memset(message, 0, BUFFER_SIZE);
    }

    return NULL;
}

void log_message(const char *username, const char *message) {
    // Get the current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    FILE *logfile = fopen("server_logs.txt", "a");
    if (logfile != NULL) {
        // Log the date, username, message, and separator
        fprintf(logfile, "Date: %s\nUser: %s\nMessage: %s\n", time_str, username, message);
        fprintf(logfile, "--------------------------------------------------\n");
        fclose(logfile);
    } else {
        perror("Could not open log file");
    }
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char username[32];
    char message[BUFFER_SIZE];
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP address
    server_addr.sin_port = htons(49688);  // Server port

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection error");
        exit(EXIT_FAILURE);
    }

    printf("Enter your username: ");
    fgets(username, 32, stdin);
    username[strlen(username) - 1] = '\0';  // Remove newline character
    send(sockfd, username, 32, 0);

    pthread_create(&tid, NULL, &receive_message, &sockfd);

    while (1) {
        fgets(message, BUFFER_SIZE, stdin);
        if (strlen(message) > 0) {
            send(sockfd, message, strlen(message), 0);
            log_message(username, message);  // Save the message to the log file
        }
        memset(message, 0, BUFFER_SIZE);
    }

    close(sockfd);
    return 0;
}

