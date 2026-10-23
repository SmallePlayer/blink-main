#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <thread>

namespace net {

int create_socket(char* type);
void return_connection(int sockfd);

void settings_server_udp(int sockfd, struct sockaddr_in* server_addr, int port);
void settings_client_udp(int sockfd, struct sockaddr_in* server_addr, int port);
void bind_socket_udp(int sockfd, struct sockaddr_in* server_addr);
void send_udp_c(int sockfd, struct sockaddr_in* client_addr, const char* message);
void receive_udp_c(int sockfd, char* buffer, size_t buffer_size, struct sockaddr_in* client_addr);
void receive_udp_int(int sockfd, int* result, struct sockaddr_in* client_addr);
void send_udp_int(int sockfd, struct sockaddr_in* client_addr, int result);

void settings_server(int sockfd, struct sockaddr_in* server_addr, int port);
int bind_socket(int sockfd, struct sockaddr_in* server_addr);
void listen_socket(int sockfd);
int accept_connection(int sockfd, struct sockaddr_in* client_addr);

void settings_client(int sockfd, struct sockaddr_in* server_addr, int port);
int connect_to_server(int sockfd, struct sockaddr_in* server_addr);

void send_int(int client, int result);
void send_c(int client, const char* message);
void receive(int client, int* result);
void receive_c(int client, char* buffer, size_t buffer_size);

template <typename T>
void receive_struct(int sock, T* data) {
    char* buf = reinterpret_cast<char*>(data);
    size_t total = 0;
    while (total < sizeof(T)) {
        ssize_t n = read(sock, buf + total, sizeof(T) - total);
        if (n == 0)  return;      // сервер закрыл соединение
        if (n < 0)   return;      // ошибка (можно проверить errno, EINTR повторить)
        total += n;
    }
}

template <typename T>
void send_struct(int sock, const T* data) {
    const char* buf = reinterpret_cast<const char*>(data);
    size_t total = 0;
    while (total < sizeof(T)) {
        ssize_t n = write(sock, buf + total, sizeof(T) - total);
        if (n <= 0) return;
        total += n;
    }
}

void send_bytes(int client, const void* data, size_t size);


void delay_seconds(int sec);
void delay_milliseconds(int ms);
void delay_microseconds(int usec);


}
