#include "net.hpp"


namespace net {

int create_socket(char* type){                  // Функция создание сокета с аргументам по типу сокета
    int sockfd;
    if (strcmp(type, "tcp") == 0) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    } else if (strcmp(type, "udp") == 0) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    } else {
        std::cerr << "Invalid socket type: " << type << std::endl;
        return -1;
    }

    if (sockfd < 0) {
        std::cerr << "Error creating socket: " << strerror(errno) << std::endl;
        return -1;
    }

    return sockfd;
}

void return_connection(int sockfd) {                        // Функция параметра для быстрого переподключения      
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Error setting socket options: " << strerror(errno) << std::endl;
    }
}

// udp server settings

void settings_server_udp(int sockfd, struct sockaddr_in* server_addr, int port) {       // Функция настроек для сервера на udp 
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;   // 0.0.0.1
    server_addr->sin_port = htons(port);
}
void settings_client_udp(int sockfd, struct sockaddr_in* server_addr, int port) {       // Функция настроек для клиента на udp 
    server_addr->sin_family = AF_INET; 
    server_addr->sin_port = htons(port); 
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
    }
}

void bind_socket_udp(int sockfd, struct sockaddr_in* server_addr) {                     // Функция для подвязки сокета к порту для udp 
    if (bind(sockfd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
    }
}

// udp send
// отправка и получение данных по udp
// отправка char* и int
void send_udp_c(int sockfd, struct sockaddr_in* client_addr, const char* message) {
    sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
}
void receive_udp_c(int sockfd, char* buffer, size_t buffer_size, struct sockaddr_in* client_addr) {
    socklen_t addrlen = sizeof(*client_addr);
    recvfrom(sockfd, buffer, buffer_size, 0, (struct sockaddr*)client_addr, &addrlen);
}
void receive_udp_int(int sockfd, int* result, struct sockaddr_in* client_addr) {
    socklen_t addrlen = sizeof(*client_addr);
    recvfrom(sockfd, result, sizeof(*result), 0, (struct sockaddr*)client_addr, &addrlen);
}
void send_udp_int(int sockfd, struct sockaddr_in* client_addr, int result) {
    sendto(sockfd, &result, sizeof(result), 0, (struct sockaddr*)client_addr, sizeof(*client_addr));
}
// tcp server settings

void settings_server(int sockfd, struct sockaddr_in* server_addr, int port) {       // Функция настроек для сервера на TCP
    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;   // 0.0.0.0
    server_addr->sin_port = htons(port);
}

int bind_socket(int sockfd, struct sockaddr_in* server_addr) {                      //Функция для подвязки порта к сокету на TCP
    if (bind(sockfd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        std::cerr << "Bind failed: " << strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

void listen_socket(int sockfd) {                                                    // Функция для прослушивания порта(настройка для сервера TCP)
    if (listen(sockfd, 1) < 0) {
        std::cerr << "Listen failed: " << strerror(errno) << std::endl;
    }
}

int accept_connection(int sockfd, struct sockaddr_in* client_addr) {                //Функция для подключения от клиента к серверу на TCP
    socklen_t addrlen = sizeof(*client_addr);
    int new_socket = accept(sockfd, (struct sockaddr*)client_addr, &addrlen);
    if (new_socket < 0) {
        std::cerr << "Accept failed: " << strerror(errno) << std::endl;
        return -1;
    }
    return new_socket;
}

// tcp client settings

void settings_client(int sockfd, struct sockaddr_in* server_addr, int port) {               // Функция для настроек для клиента на TCP
    server_addr->sin_family = AF_INET; 
    server_addr->sin_port = htons(port); 
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr->sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
    }
}

int connect_to_server(int sockfd, struct sockaddr_in* server_addr) {                        // Функция подключения клиента к серверу по порту
    if (connect(sockfd, (struct sockaddr*)server_addr, sizeof(*server_addr)) < 0) {
        std::cerr << "Connection failed: " << strerror(errno) << std::endl;
        return -1;
    }
    return 0;
}

// tcp send 
// отправка и получение данных по TCP
void send_int(int client, int result){
    write(client, &result, sizeof(result));
}
void send_c(int client, const char* message){
    write(client, message, strlen(message));
}
void receive_int(int client, int* result){
    read(client, result, sizeof(*result));
}
void receive_c(int client, char* buffer, size_t buffer_size){
    read(client, buffer, buffer_size);
}

void send_bytes(int client, const void* data, size_t size) {
    write(client, data, size);
}


// delay

void delay_seconds(int sec)
{
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

void delay_microseconds(int usec)
{
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

void delay_milliseconds(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}


} // namespace net
