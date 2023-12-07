/*
 *
 * This is Based on the Code of Lewis Van Winkle
 * Presented in "Hands-On Network Programming with C (2018)"
 *
 */

#include "multi_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define BUFFER_SIZE 1024
#define PORT "8081"

char *get_client_ip(SOCKET socket)
{
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    char *ip;

    if (getpeername(socket, (struct sockaddr *)&client_address, &client_len) == 0)
    {

        if (client_address.ss_family == AF_INET)
        {

            // Allocate memory
            ip = malloc(INET_ADDRSTRLEN);
            if (ip == NULL)
            {
                printf("Error allocation memory for IP\n");
                exit(EXIT_FAILURE);
            }
            // Casting to sockaddr_in
            struct sockaddr_in *s = (struct sockaddr_in *)&client_address;
            // Network to presentation: convert ip readable
            inet_ntop(AF_INET, &(s->sin_addr), ip, INET_ADDRSTRLEN);
        }
        else if (client_address.ss_family == AF_INET6)
        {
            // Allocate memory
            ip = malloc(INET6_ADDRSTRLEN);
            if (ip == NULL)
            {
                printf("Error allocation memory for IP\n");
                exit(EXIT_FAILURE);
            }
            // Casting to sockaddr_in
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client_address;
            // Network to presentation: convert ip readable
            inet_ntop(AF_INET6, &(s->sin6_addr), ip, INET_ADDRSTRLEN);
        }
        return ip;
    }
    else
    {
        printf("Error obtaining IP from Socket %d\n", socket);
        exit(EXIT_FAILURE);
    }
}

int main()
{

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d))
    {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, PORT, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
                           bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen))
    {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
             bind_address->ai_addr, bind_address->ai_addrlen))
    {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0)
    {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while (1)
    {
        fd_set reads;
        reads = master;
        if (select(max_socket + 1, &reads, 0, 0, 0) < 0)
        {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        SOCKET i;
        for (i = 1; i <= max_socket; ++i)
        {
            if (FD_ISSET(i, &reads))
            {

                // New connection, we need to "copy" the socket_listen
                if (i == socket_listen)
                {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    SOCKET socket_client = accept(socket_listen,
                                                  (struct sockaddr *)&client_address,
                                                  &client_len);
                    if (!ISVALIDSOCKET(socket_client))
                    {
                        fprintf(stderr, "accept() failed. (%d)\n",
                                GETSOCKETERRNO());
                        return 1;
                    }

                    FD_SET(socket_client, &master);
                    if (socket_client > max_socket)
                        max_socket = socket_client;

                    char address_buffer[100];
                    getnameinfo((struct sockaddr *)&client_address,
                                client_len,
                                address_buffer, sizeof(address_buffer), 0, 0,
                                NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);

                    // Connection is established then we receive and send data
                }
                else
                {
                    // Buffer
                    char read[BUFFER_SIZE];
                    // We are receiving this
                    int bytes_received = recv(i, read, BUFFER_SIZE, 0);
                    if (bytes_received < 1)
                    {
                        FD_CLR(i, &master);
                        CLOSESOCKET(i);
                        continue;
                    }

                    // Send the data received by socket i to others sockets
                    SOCKET j;
                    for (j = 1; j <= max_socket; ++j)
                    {
                        if (FD_ISSET(j, &master))
                        {
                            if (j == socket_listen || j == i)
                                continue;
                            else
                            {
                                // get client IP
                                char *client_ip = get_client_ip(i);
                                // Format "[From: ip]"
                                char FORMAT[] = "[From: %s]: %s";
                                size_t format_length = strlen(FORMAT);
                                // Buffer To Send
                                size_t total_size = format_length + strlen(client_ip) + bytes_received;
                                char send_buffer[total_size + 1];
                                // Format
                                snprintf(send_buffer, sizeof(send_buffer), FORMAT, client_ip, read);
                                // Sent Data
                                send(j, send_buffer, total_size, 0);
                                // Free allocated memory
                                free(client_ip);
                            }
                        }
                    }
                    // Clear buffer
                    memset(read, 0, BUFFER_SIZE);
                }
            } // if FD_ISSET
        }     // for i to max_socket
    }         // while(1)

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");

    return EXIT_SUCCESS;
}
