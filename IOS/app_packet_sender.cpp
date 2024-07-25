#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <random>

class Packet {
public:
    uint32_t pkt_type;
    float temperatureBitPattern;

    Packet(uint32_t pktType, float temperature) {
        pkt_type = pktType;
        temperatureBitPattern = temperature;
    }

    void toBytes(char* buffer) const {
        memcpy(buffer, &pkt_type, sizeof(pkt_type));
        memcpy(buffer + sizeof(pkt_type), &temperatureBitPattern, sizeof(temperatureBitPattern));
    }
};

void sendPacket(int sockfd, const sockaddr_in& servaddr, const Packet& packet) {
    char buffer[sizeof(Packet)];
    packet.toBytes(buffer);

    ssize_t sentBytes = sendto(sockfd, buffer, sizeof(buffer), 0, (const struct sockaddr*)&servaddr, sizeof(servaddr));
    if (sentBytes < 0) {
        std::cerr << "Error sending packet: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Packet sent: " << std::endl;
        std::cout << "  Packet Type: " << packet.pkt_type << std::endl;
        float temperature = *reinterpret_cast<const float*>(&packet.temperatureBitPattern);
        std::cout << "  Temperature: " << temperature << std::endl;
    }
}

float generateRandomTemperature() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(-50.0, 50.0);  // Example temperature range from -50.0 to 50.0
    return dis(gen);
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(3001);
    servaddr.sin_addr.s_addr = inet_addr("192.168.0.19");  // Change this to the target IP address

    uint32_t pktType = 1;  // Example packet type

    while (true) {
        float temperature = generateRandomTemperature();
        Packet packet(pktType, temperature);
        sendPacket(sockfd, servaddr, packet);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close(sockfd);
    return 0;
}