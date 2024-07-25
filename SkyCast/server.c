#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "jsmn.h"
#include <curl/curl.h>

#define UNIX_SOCKET_PATH "/tmp/weather_socket"
#define INET_SOCKET_PORT 8080
#define MAX_CLIENTS 10

pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;
int unixCount = 0;
int client_fds[MAX_CLIENTS]; // Array to store client file descriptors

#define API_KEY "0a34b9e852e44c048d7a966df65c8504"
#define API_KEY_WEATHERAPI "92b398d087eb4b45bd6184403231306"

size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total_size = size * nmemb;
    memcpy(userdata, ptr, total_size);
    return total_size;
}

int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

char *get_weather_data(const char *location) {
    // Construct the URL for the Weatherbit API request
    const char *base_url = "https://api.weatherbit.io/v2.0/current";
    char url[1024];
    snprintf(url, sizeof(url), "%s?key=%s&city=%s", base_url, API_KEY, location);

    // Initialize libcurl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: could not initialize libcurl\n");
        return NULL;
    }

    // Set the URL for the request
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Response buffer
    char response_buffer[4096];
    memset(response_buffer, 0, sizeof(response_buffer));

    // Configure libcurl to write response to the buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

    // Send the request and retrieve the response
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Cleanup libcurl
    curl_easy_cleanup(curl);

    // Parse the JSON response using jsmn
    jsmn_parser parser;
    jsmntok_t tokens[1024];  // Adjust the size depending on your expected JSON structure
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, response_buffer, strlen(response_buffer), tokens, sizeof(tokens) / sizeof(tokens[0]));

    // Check if parsing was successful
    if (token_count < 0) {
        fprintf(stderr, "Error: failed to parse JSON response\n");
        return NULL;
    }

    // Find the temperature and apparent temperature tokens in the JSON response
    const char *temperature_key = "temp";
    const char *apparent_temperature_key = "app_temp";
    const char *wind_key = "wind_spd";
    const char *rain_key = "precip";
    char temperature_value[16];
    char apparent_temperature_value[16];
    char wind_value[16];
    char rain_value[16];
    memset(temperature_value, 0, sizeof(temperature_value));
    memset(apparent_temperature_value, 0, sizeof(apparent_temperature_value));
    memset(wind_value, 0, sizeof(wind_value));
    memset(rain_value, 0, sizeof(rain_value));

    for (int i = 0; i < token_count - 1; i++) {
        if (jsoneq(response_buffer, &tokens[i], temperature_key) == 0) {
            snprintf(temperature_value, sizeof(temperature_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], apparent_temperature_key) == 0) {
            snprintf(apparent_temperature_value, sizeof(apparent_temperature_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], wind_key) == 0) {
            snprintf(wind_value, sizeof(wind_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], rain_key) == 0) {
            snprintf(rain_value, sizeof(rain_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        }
    }

    // Construct the weather data response
    char *weather_data = (char *)malloc(1024 * sizeof(char));
    snprintf(weather_data, 1024, "Temperature: %s °C\nApparent Temperature: %s °C\nWind: %s km/h\nPrecipitaion: %s mm\n", temperature_value, apparent_temperature_value, wind_value, rain_value);

    return weather_data;
}

char *get_air_quality_data(const char *location) {
    // Construct the URL for the Weatherbit Air Quality API request
    const char *base_url = "https://api.weatherbit.io/v2.0/current/airquality";
    char url[1024];
    snprintf(url, sizeof(url), "%s?key=%s&city=%s", base_url, API_KEY, location);

    // Initialize libcurl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: could not initialize libcurl\n");
        return NULL;
    }

    // Set the URL for the request
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Response buffer
    char response_buffer[4096];
    memset(response_buffer, 0, sizeof(response_buffer));

    // Configure libcurl to write response to the buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

    // Send the request and retrieve the response
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Cleanup libcurl
    curl_easy_cleanup(curl);

    // Parse the JSON response using jsmn
    jsmn_parser parser;
    jsmntok_t tokens[1024];  // Adjust the size depending on your expected JSON structure
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, response_buffer, strlen(response_buffer), tokens, sizeof(tokens) / sizeof(tokens[0]));

    // Check if parsing was successful
    if (token_count < 0) {
        fprintf(stderr, "Error: failed to parse JSON response\n");
        return NULL;
    }

    // Find the air quality tokens in the JSON response
    const char *aqi_key = "aqi";
    char aqi_value[16];
    memset(aqi_value, 0, sizeof(aqi_value));

    for (int i = 0; i < token_count - 1; i++) {
        if (jsoneq(response_buffer, &tokens[i], aqi_key) == 0) {
            snprintf(aqi_value, sizeof(aqi_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        }
    }

    // Construct the air quality data response
    char *air_quality_data = (char *)malloc(1024 * sizeof(char));
    snprintf(air_quality_data, 1024, "AQI: %s\n", aqi_value);

    return air_quality_data;
}

char *get_historic_weather(const char *location, const char *date) {
    // Construct the URL for the WeatherAPI request
    const char *base_url = "https://api.weatherapi.com/v1/history.json";
    char url[1024];
    snprintf(url, sizeof(url), "%s?key=%s&q=%s&dt=%s&hour=12", base_url, API_KEY_WEATHERAPI, location, date);

    // Initialize libcurl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: could not initialize libcurl\n");
        return NULL;
    }

    // Set the URL for the request
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Response buffer
    char response_buffer[4096];
    memset(response_buffer, 0, sizeof(response_buffer));

    // Configure libcurl to write response to the buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

    // Send the request and retrieve the response
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Cleanup libcurl
    curl_easy_cleanup(curl);

    // Parse the JSON response using jsmn
    jsmn_parser parser;
    jsmntok_t tokens[1024];  // Adjust the size depending on your expected JSON structure
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, response_buffer, strlen(response_buffer), tokens, sizeof(tokens) / sizeof(tokens[0]));

    // Check if parsing was successful
    if (token_count < 0) {
        fprintf(stderr, "Error: failed to parse JSON response\n");
        return NULL;
    }

    // Find the temperature, feels like temperature, wind speed, and precipitation tokens in the JSON response
    const char *temperature_key = "temp_c";
    const char *feels_like_key = "feelslike_c";
    const char *wind_speed_key = "wind_kph";
    const char *precipitation_key = "precip_mm";

    char temperature_value[16];
    char feels_like_value[16];
    char wind_speed_value[16];
    char precipitation_value[16];

    memset(temperature_value, 0, sizeof(temperature_value));
    memset(feels_like_value, 0, sizeof(feels_like_value));
    memset(wind_speed_value, 0, sizeof(wind_speed_value));
    memset(precipitation_value, 0, sizeof(precipitation_value));

    for (int i = 0; i < token_count - 1; i++) {
        if (jsoneq(response_buffer, &tokens[i], temperature_key) == 0) {
            snprintf(temperature_value, sizeof(temperature_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], feels_like_key) == 0) {
            snprintf(feels_like_value, sizeof(feels_like_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], wind_speed_key) == 0) {
            snprintf(wind_speed_value, sizeof(wind_speed_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], precipitation_key) == 0) {
            snprintf(precipitation_value, sizeof(precipitation_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        }
    }

    // Construct the historic weather data response
    char *historic_weather_data = (char *)malloc(1024 * sizeof(char));
    snprintf(historic_weather_data, 1024, "Temperature: %s °C\nApparent Temperature: %s °C\nWind: %s km/h\nPrecipitaion: %s mm\n",
             temperature_value, feels_like_value, wind_speed_value, precipitation_value);

    return historic_weather_data;
}

char *get_future_weather(const char *location, const char *date) {
    // Construct the URL for the WeatherAPI request
    const char *base_url = "https://api.weatherapi.com/v1/future.json";
    char url[1024];
    snprintf(url, sizeof(url), "%s?key=%s&q=%s&dt=%s&hour=12", base_url, API_KEY_WEATHERAPI, location, date);

    // Initialize libcurl
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: could not initialize libcurl\n");
        return NULL;
    }

    // Set the URL for the request
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // Response buffer
    char response_buffer[4096];
    memset(response_buffer, 0, sizeof(response_buffer));

    // Configure libcurl to write response to the buffer
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

    // Send the request and retrieve the response
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: curl request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Cleanup libcurl
    curl_easy_cleanup(curl);

    // Parse the JSON response using jsmn
    jsmn_parser parser;
    jsmntok_t tokens[1024];  // Adjust the size depending on your expected JSON structure
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, response_buffer, strlen(response_buffer), tokens, sizeof(tokens) / sizeof(tokens[0]));

    // Check if parsing was successful
    if (token_count < 0) {
        fprintf(stderr, "Error: failed to parse JSON response\n");
        return NULL;
    }

    // Find the temperature, feels like temperature, wind speed, and precipitation tokens in the JSON response
    const char *temperature_key = "maxtemp_c";
    const char *feels_like_key = "mintemp_c";
    const char *wind_speed_key = "maxwind_kph";
    const char *precipitation_key = "totalprecip_mm";

    char temperature_value[16];
    char feels_like_value[16];
    char wind_speed_value[16];
    char precipitation_value[16];

    memset(temperature_value, 0, sizeof(temperature_value));
    memset(feels_like_value, 0, sizeof(feels_like_value));
    memset(wind_speed_value, 0, sizeof(wind_speed_value));
    memset(precipitation_value, 0, sizeof(precipitation_value));

    for (int i = 0; i < token_count - 1; i++) {
        if (jsoneq(response_buffer, &tokens[i], temperature_key) == 0) {
            snprintf(temperature_value, sizeof(temperature_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], feels_like_key) == 0) {
            snprintf(feels_like_value, sizeof(feels_like_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], wind_speed_key) == 0) {
            snprintf(wind_speed_value, sizeof(wind_speed_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        } else if (jsoneq(response_buffer, &tokens[i], precipitation_key) == 0) {
            snprintf(precipitation_value, sizeof(precipitation_value), "%.*s",
                     tokens[i + 1].end - tokens[i + 1].start, response_buffer + tokens[i + 1].start);
        }
    }

    // Construct the future weather data response
    char *future_weather_data = (char *)malloc(1024 * sizeof(char));
    snprintf(future_weather_data, 1024, "Max Temperature: %s °C\nMin Temperature: %s °C\nWind: %s km/h\nPrecipitaion: %s mm\n",
             temperature_value, feels_like_value, wind_speed_value, precipitation_value);

    return future_weather_data;
}



void handle_unix_client(int client_fd) {
    // Read client request
    int i = 1;
    while(i){
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    if (read(client_fd, buffer, sizeof(buffer) - 1) == -1) {
        fprintf(stderr, "Error: could not read from UNIX client socket: %s\n", strerror(errno));
        close(client_fd);
        return;
    }

    // Process request and send response
    char *response = NULL;
    if (strcmp(buffer, "get_client_count") == 0) {
        pthread_mutex_lock(&client_count_mutex);
        int count = client_count;
        pthread_mutex_unlock(&client_count_mutex);

        // Convert the client count to a string
        char count_str[20];
        sprintf(count_str, "%d", count);
        response = strdup(count_str);
    } else if (strcmp(buffer, "stop") == 0) {
        response = "Server will stop ...";
        exit(EXIT_SUCCESS);
    } else if (strcmp(buffer, "exit") == 0) {
        i = 0;
        unixCount = 0;
    }   
    else {
        // If we receive an unknown request, respond with an error message
        response = "Error: invalid request";
    }

    if (response != NULL) {
        if (write(client_fd, response, strlen(response)) == -1) {
            fprintf(stderr, "Error: could not write response to UNIX client socket: %s\n", strerror(errno));
        }
        free(response);
    }
    }
    printf("Admin disconnected!\n");
    close(client_fd);
}

void *unix_client_handler(void *arg) {

    
    int server_fd, client_fd, len;
    struct sockaddr_un server_addr, client_addr;

   

    printf("Server started. Waiting for UNIX clients...\n");

    // Accept connections and process client requests
    while (true) {
        // Wait for a client connection
        if (unixCount == 0) {


/////////////////////


  if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error: could not create UNIX socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set server properties
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, UNIX_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // Remove any existing socket file
    unlink(UNIX_SOCKET_PATH);

    // Bind the socket to the server address
    len = offsetof(struct sockaddr_un, sun_path) + strlen(server_addr.sun_path);
    if (bind(server_fd, (struct sockaddr *)&server_addr, len) == -1) {
        fprintf(stderr, "Error: could not bind UNIX socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(server_fd, 1) == -1) {
        fprintf(stderr, "Error: could not listen on UNIX socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }


/////////////////////


        socklen_t client_len = sizeof(struct sockaddr_un);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1 && unixCount == 1) {
            fprintf(stderr, "Error: could not accept UNIX client connection: %s\n", strerror(errno));
            continue;
        }

        
        // Print a message for each connected client
        printf("Admin connected through UNIX socket\n");
        unixCount = 1;
        close(server_fd);
        
        // Handle the client in a separate thread
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, (void *(*)(void *))handle_unix_client, (void *)(intptr_t)client_fd) != 0 && unixCount == 1) {
            fprintf(stderr, "Error: could not create UNIX client handler thread: %s\n", strerror(errno));
            close(client_fd);
            continue;
        }
        pthread_detach(client_thread);
        }
        
    }
    return NULL;
    }




void handle_inet_client(int client_fd) {
    // Read client request
    int i = 1;
    while (i) {
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        if (read(client_fd, buffer, sizeof(buffer) - 1) == -1) {
            fprintf(stderr, "Error: could not read from INET client socket: %s\n", strerror(errno));
            close(client_fd);
            return;
        }

        // Process request and send response
        char *response = NULL;
        if (strcmp(buffer, "get_current_weather") == 0) {
            char location[100];
            memset(location, 0, sizeof(location));
            if (read(client_fd, location, sizeof(location) - 1) == -1) {
                fprintf(stderr, "Error: could not read location from INET client socket: %s\n", strerror(errno));
                //close(client_fd);
                return;
            }
            response = get_weather_data(location);
        } else if (strcmp(buffer, "get_air_quality") == 0) {
            char location_aqi[100];
            memset(location_aqi, 0, sizeof(location_aqi));
            if (read(client_fd, location_aqi, sizeof(location_aqi) - 1) == -1) {
                fprintf(stderr, "Error: could not read location from INET client socket: %s\n", strerror(errno));
               // close(client_fd);
                return;
            }
            response = get_air_quality_data(location_aqi);
        } else if (strcmp(buffer, "get_historic_weather") == 0) {
            char location_history[100];
            char date_history[20];

            memset(location_history, 0, sizeof(location_history));
            if (read(client_fd, location_history, sizeof(location_history) - 1) == -1) {
                fprintf(stderr, "Error: could not read location from INET client socket: %s\n", strerror(errno));
                //close(client_fd);
                return;
            }

            memset(date_history, 0, sizeof(date_history));
            if (read(client_fd, date_history, sizeof(date_history) - 1) == -1) {
                fprintf(stderr, "Error: could not read date from INET client socket: %s\n", strerror(errno));
                //close(client_fd);
                return;
            }

            response = get_historic_weather(location_history, date_history);
        } else if (strcmp(buffer, "get_future_weather") == 0) {
            char location_future[100];
            char date_future[20];

            memset(location_future, 0, sizeof(location_future));
            if (read(client_fd, location_future, sizeof(location_future) - 1) == -1) {
                fprintf(stderr, "Error: could not read location from INET client socket: %s\n", strerror(errno));
                //close(client_fd);
                return;
            }

            memset(date_future, 0, sizeof(date_future));
            if (read(client_fd, date_future, sizeof(date_future) - 1) == -1) {
                fprintf(stderr, "Error: could not read date from INET client socket: %s\n", strerror(errno));
                //close(client_fd);
                return;
            }

            response = get_future_weather(location_future, date_future);
        } else if (strcmp(buffer, "exit") == 0) {
            i = 0;
            // Remove client file descriptor from the array
            pthread_mutex_lock(&client_count_mutex);
            for (int i = 0; i < client_count; i++) {
                if (client_fds[i] == client_fd) {
                    for (int j = i; j < client_count - 1; j++) {
                        client_fds[j] = client_fds[j + 1];
                    }
                    break;
                }
            }
            client_count--;
            pthread_mutex_unlock(&client_count_mutex);

            close(client_fd);
        }
         else {
            response = "Error: invalid request";
        }

        if (response != NULL) {
            if (write(client_fd, response, strlen(response)) == -1) {
                fprintf(stderr, "Error: could not write response to INET client socket: %s\n", strerror(errno));
            }
            free(response);
        }
    }
    close(client_fd);
}


void *inet_client_handler(void *arg) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;

    // Create an INET socket for client connection
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        fprintf(stderr, "Error: could not create INET socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Set server properties
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(INET_SOCKET_PORT);

    // Bind the socket to the server address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "Error: could not bind INET socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Listen for client connections
    if (listen(server_fd, 5) == -1) {
        fprintf(stderr, "Error: could not listen on INET socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    printf("Server started. Waiting for INET clients...\n");

    // Accept connections and process client requests
    while (true) {
        // Wait for a client connection
        socklen_t client_len = sizeof(struct sockaddr_in);
        if ((client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len)) == -1) {
            fprintf(stderr, "Error: could not accept INET client connection: %s\n", strerror(errno));
            continue;
        }

        // Add client file descriptor to the array
        pthread_mutex_lock(&client_count_mutex);
        if (client_count < MAX_CLIENTS) {
            client_fds[client_count] = client_fd;
            client_count++;
        }
        pthread_mutex_unlock(&client_count_mutex);

        // Print a message for each connected client
        printf("Client connected through INET socket.Client number: %d\n", client_count);

        // Handle the client in a separate thread
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, (void *(*)(void *))handle_inet_client, (void *)(intptr_t)client_fd) != 0) {
            fprintf(stderr, "Error: could not create INET client handler thread: %s\n", strerror(errno));

            // Remove client file descriptor from the array
            pthread_mutex_lock(&client_count_mutex);
            for (int i = 0; i < client_count; i++) {
                if (client_fds[i] == client_fd) {
                    for (int j = i; j < client_count - 1; j++) {
                        client_fds[j] = client_fds[j + 1];
                    }
                    break;
                }
            }
            client_count--;
            pthread_mutex_unlock(&client_count_mutex);

            close(client_fd);
            continue;
        }
        pthread_detach(client_thread);
    }

    return NULL;
}

int main() {
    // Initialize the client file descriptor array
    int client_fds[MAX_CLIENTS];
    memset(client_fds, 0, sizeof(client_fds));

    // Initialize the client count mutex and variable
    pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
    int client_count = 0;

    // Initialize the UNIX client thread
    pthread_t unix_client_thread;
    if (pthread_create(&unix_client_thread, NULL, unix_client_handler, NULL) != 0) {
        fprintf(stderr, "Error: could not create UNIX client thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Initialize the INET client thread
    pthread_t inet_client_thread;
    if (pthread_create(&inet_client_thread, NULL, inet_client_handler, NULL) != 0) {
        fprintf(stderr, "Error: could not create INET client thread: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Wait for the program to finish
    pthread_join(unix_client_thread, NULL);
    pthread_join(inet_client_thread, NULL);

    return 0;
}
