#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define UNIX_SOCKET_PATH "/tmp/weather_socket"

int client_fd;

void clearConsole() {
    printf("\033[2J\033[H");
}

void displayMenu() {
    printf("\nMenu:\n");
    printf("1. Get client count\n");
    printf("2. Stop the server\n");
    printf("0. Exit\n");
    printf("===========================\n");
    printf("Enter your choice: ");
}

void handle_response(const char *response) {
    printf("\nServer response: %s\n", response);
}

void send_request(const char *request) {
    // Send request to the server
    if (write(client_fd, request, strlen(request)) == -1) {
        fprintf(stderr, "Error: could not send request to UNIX server: %s\n", strerror(errno));
        close(client_fd);
        exit(EXIT_FAILURE);
        
    }

    // Read response from the server
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    if (read(client_fd, buffer, sizeof(buffer) - 1) == -1) {
        fprintf(stderr, "Error: could not read response from UNIX server: %s\n", strerror(errno));
        close(client_fd);
        exit(EXIT_FAILURE);
        
    }

    // Handle the server response
    handle_response(buffer);
}

int main() {
    struct sockaddr_un server_addr;

    // Create a UNIX socket for server connection
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error: could not create UNIX socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set server properties
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UNIX_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Connect to the server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Error: could not connect to UNIX server: %s\n", strerror(errno));
        close(client_fd);
        exit(EXIT_FAILURE);
        return 0;
    }

    int choice;
    char request[20];

    clearConsole();
    while (1) {
        displayMenu();
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1:
                strcpy(request, "get_client_count");
                clearConsole();
                send_request(request);
                continue;
            case 2:
                printf("Shut down the server...\n");
                strcpy(request, "stop");
                clearConsole();
                printf("The server will shut down ...");
                sleep(2);
                send_request(request);
                clearConsole();
                exit(EXIT_SUCCESS);
            case 0:
                printf("Exiting...\n");
                strcpy(request, "exit");
                send_request(request);
                clearConsole();
                close(client_fd); // Close the connection
                exit(EXIT_SUCCESS);
            default:
                printf("Invalid choice. Please try again.\n");
        }
    }

    return 0;
}
