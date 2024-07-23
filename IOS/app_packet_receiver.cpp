#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

class Packet {
public:
    uint32_t pkt_type;
    float valueBits;
};

void printPacket(const Packet& packet) {
    std::cout << "Received packet:" << std::endl;
    std::string type = "";
    switch (packet.pkt_type) {
        case 2: 
            type = "Temperature Control Packet";
            break;
        
        case 3: 
            type = "Motor Control Packet";
            break;
        
        case 4: 
            type = "Vent Shutoff Packet";
            break;
    }
    std::cout << "Packet Type: " << type << std::endl;
    float value = packet.valueBits;
    std::cout << "Value: " << value << std::endl;
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(1234);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(sockfd);
        return -1;
    }

    std::cout << "Listening on port 1234..." << std::endl;

    char buffer[sizeof(Packet)];
    socklen_t len;
    int n;

    while (true) {
        n = recvfrom(sockfd, buffer, sizeof(buffer), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            std::cerr << "Receive failed" << std::endl;
            close(sockfd);
            return -1;
        }

        Packet packet;
        std::memcpy(&packet.pkt_type, buffer, sizeof(packet.pkt_type));
        std::memcpy(&packet.valueBits, buffer + sizeof(packet.pkt_type), sizeof(packet.valueBits));

        printPacket(packet);
    }

    close(sockfd);
    return 0;
}