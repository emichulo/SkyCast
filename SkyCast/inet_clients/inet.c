#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>

#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 8080

void clearConsole() {
    printf("\033[2J\033[H");
}


void displayMenu() {
    printf("\nMenu:\n");
    printf("1. Get current weather\n");
    printf("2. Get air quality\n");
    printf("3. Get historic weather\n");
    printf("4. Get future weather\n");
    printf("5. Exit\n");
    printf("===========================\n");
    printf("Enter your choice: ");
}

void sendRequest(int socket, const char *request) {
    // Send the request to the server
    if (write(socket, request, strlen(request)) == -1) {
        fprintf(stderr, "Error: could not write request to server: %s\n", strerror(errno));
    }
}

void sendLocation(int socket, const char *location) {
    // Send the location to the server
    if (write(socket, location, strlen(location)) == -1) {
        fprintf(stderr, "Error: could not write location to server: %s\n", strerror(errno));
    }
}

void sendDate(int socket, const char *start_date) {
    // Send the start_date to the server
    if (write(socket, start_date, strlen(start_date)) == -1) {
        fprintf(stderr, "Error: could not write date to server: %s\n", strerror(errno));
    }
}

void readResponse(int socket) {
    // Read the response from the server
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    if (read(socket, buffer, sizeof(buffer) - 1) == -1) {
        fprintf(stderr, "Error: could not read response from server: %s\n", strerror(errno));
    } else {
        printf("Response from server:\n%s\n", buffer);
    }
}

int isDateValidFuture(const char* date) {
    // Check if the date is valid (between 14 and 300 days from today)
    time_t current_time = time(NULL);
    struct tm current_date = *localtime(&current_time);

    struct tm input_date = {0};
    if (strptime(date, "%Y-%m-%d", &input_date) == NULL) {
        fprintf(stderr, "Error: Invalid date format. Please use the format YYYY-MM-DD.\n");
        return 0;
    }

    time_t input_time = mktime(&input_date);
    double seconds_difference = difftime(input_time, current_time);
    double days_difference = seconds_difference / (60 * 60 * 24);

    if (days_difference < 14 || days_difference > 300) {
        fprintf(stderr, "Error: Invalid date. The provided date should be between 14 and 300 days from today.\n");
        return 0;
    }

    return 1;
}

int isDateValidHistory(const char *date) {
    // Check if the date is valid (not older than 365 days and not greater than today)
    time_t current_time = time(NULL);
    struct tm current_date = *localtime(&current_time);

    struct tm input_date = {0};
    if (strptime(date, "%Y-%m-%d", &input_date) == NULL) {
        fprintf(stderr, "Error: Invalid date format. Please use the format YYYY-MM-DD.\n");
        return 0;
    }

    time_t diff = difftime(current_time, mktime(&input_date));
    double days_difference = diff / (60 * 60 * 24);

    if (days_difference > 365) {
        fprintf(stderr, "Error: Invalid date. The provided date is older than 365 days.\n");
        return 0;
    }

    if (mktime(&input_date) > current_time) {
        fprintf(stderr, "Error: Invalid date. The provided date is in the future.\n");
        return 0;
    }

    return 1;
}

int main() {
    int socket_fd;
    struct sockaddr_in server_addr;

    // Create an INET socket for the client
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error: could not create socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set server properties
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &(server_addr.sin_addr)) <= 0) {
        fprintf(stderr, "Error: invalid server address\n");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Error: could not connect to server: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    int choice;
    char request[1024];
    char location[100];
    char location_aqi[100];
    char location_history[100];
    char location_future[100];
    char date_history[20];
    char date_future[20];

    clearConsole();
    while (1) {
        displayMenu();
        scanf("%d", &choice);
        getchar();  // Consume newline character

        switch (choice) {
            case 1:
                sendRequest(socket_fd, "get_current_weather");

                printf("Enter location: ");
                scanf("%s", location);
                clearConsole();
                sendLocation(socket_fd, location);

                readResponse(socket_fd);
                break;
            case 2:
                sendRequest(socket_fd, "get_air_quality");

                printf("Enter location: ");
                scanf("%s", location_aqi);
                clearConsole();
                sendLocation(socket_fd, location_aqi);

                readResponse(socket_fd);
                break;
            case 3:
                sendRequest(socket_fd, "get_historic_weather");
                
                clearConsole();
                printf("Enter location: ");
                scanf("%s", location_history);
                sendLocation(socket_fd, location_history);


                printf("Enter date: ");
                scanf("%s", date_history);
                sendDate(socket_fd, date_history);


                if (!isDateValidHistory(date_history)) {
                    // Date is not valid, go back to the menu
                    break;
                }

                readResponse(socket_fd);
                break;

            case 4:
                sendRequest(socket_fd, "get_future_weather");
                clearConsole();
                printf("Enter location: ");
                scanf("%s", location_future);
                sendLocation(socket_fd, location_future);

                printf("Enter date: ");
                scanf("%s", date_history);
                sendDate(socket_fd, date_history);

                if (!isDateValidFuture(date_history)) {
                    // Date is not valid, go back to the menu
                    break;
                }

                readResponse(socket_fd);
                break;
            case 5:
                sendRequest(socket_fd, "exit");
                printf("Exiting...\n");
                close(socket_fd);
                exit(EXIT_SUCCESS);
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    }

    return 0;
}