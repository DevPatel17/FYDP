#include <iostream>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <random>


#define PORT 8080
using namespace std;

// Function to generate a random float between min and max (inclusive)
float randomFloat(float min, float max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

class Packet{
    public:
        int pkt_type;
        float temperature;
        // unsigned int motor_pos;

};

int main(int argc, char const *argv[]) {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    // char *hello = "Hello from client";
    Packet data;
    data.pkt_type = 1;
    data.temperature = 22.0;
    

    char buffer[1024] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // std::cout << "Waiting to connect" << std::endl;
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    for(int i = 0; i < 25; i++){
        data.temperature = randomFloat(20.0, 25.0);
        
        cout << "Send: " << send(sock, &data, sizeof(data), 0) << endl;
        
        valread = read(sock, buffer, 1024);
        
        cout << "Recv: " << valread << endl;
    }
    return 0;
}