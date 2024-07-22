#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <pthread.h> 
#include <queue> 

unsigned int vent_ID = 0;
using namespace std;
#define PORT 8080
#define IP_ADDR "192.168.1.1"
#define DATA_PACKET 0x1
#define CONTROL_PACKET 0x2
#define NUM_VENTS 10;
#define DESIRED_TEMP 23.0

class Vent{
    public:
        unsigned ID;
        float temperature;
        float desired_temperature;
        unsigned cover;
        bool user_forced;
        // Default constructor
        Vent() : ID(0), temperature(0.0f), desired_temperature(23.0f), cover(0), user_forced(false) {
        // Optionally, you can initialize each member explicitly in the constructor initializer list
    }
};

class Packet{
    public:
        int pkt_type;
        float temperature;
};

int server_fd, new_socket, valread;
struct sockaddr_in address;
int opt = 1;
int addrlen = sizeof(address);
vector<int> client_sockets;
pthread_t vent_recv_threads[10];
Vent vent_arr[10];

double Kp = 1.5;  // Proportional gain
double Ki = 0.5; // Integral gain
double Kd = 0.05; // Derivative gain

double integral = 0;
double previous_error = 0;

int update_cover(int curr_temp, int desired_temp){
    double error = desired_temp - curr_temp;
    
    // Proportional term
    double proportional = Kp * error;
    
    // Integral term
    integral += error;
    double integral_term = Ki * integral;
    
    // Derivative term
    double derivative = Kd * (error - previous_error);
    previous_error = error;
    
    // Calculate PID output
    double output = proportional + integral_term + derivative;
    
    // Limit the output to the range [0, 10]
    if (output > 10) {
        output = 10;
    } else if (output < 0) {
        output = 0;
    }
    
    return output;
}


bool parse_packet(char *recv_buffer, Packet &data) {
    if(recv_buffer == NULL){
        cout << "Buffer is NULL" << endl;
        return false;
    }

    //Need to do error checking here
    data.pkt_type = *recv_buffer;
    recv_buffer += sizeof(int);
    data.temperature = *((float *)recv_buffer);

    return true;
}

void* recv_packets(void *args){
    
    int sockfd = *(int *)args;
    unsigned int vent_num = *(unsigned int*)((char *)args + sizeof(int));

    char buffer[1024];
    while(1){

        // Receive data from the client
        // cout << "Waiting to recieve data..." << endl;
        valread = recv(sockfd, buffer, 1024, 0);
        if (valread == 0) {
            continue;
        } else if (valread < 0) {
            perror("recv failed");
        } else {
            std::cout << "Received: " << valread << std::endl;
            Packet data;
            if(!parse_packet(buffer, data)){
                cout << "Packet parsing error" << endl;
                continue;
            }

            cout << "pkt type: " << (int)data.pkt_type << endl;
            if(data.pkt_type != DATA_PACKET){
                cout << "Not a data packet" << endl;
                continue;
            }

            cout << "Temp recvd: " << data.temperature << endl;

            vent_arr[vent_num].temperature = data.temperature;
            
            int new_cover = update_cover(data.temperature, DESIRED_TEMP);

            if(new_cover != vent_arr[vent_num].cover)
                vent_arr[vent_num].cover = new_cover;

            //send packet back

        }
    }
    return NULL;
}

void* connection_setup(void *args){
    while(1){
        //check for setup connections
        //This is a blocking call
        cout << "Waiting for new connection..." << endl;
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Add the new socket to our list of client sockets
        client_sockets.push_back(new_socket);

        // Print IP and Port of the connected client
        std::cout << "New connection from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;

        // Add the new socket to our list of client sockets
        client_sockets.push_back(new_socket);

        // Print IP and Port of the connected client
        std::cout << "New connection from " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << std::endl;
        
        std::unique_ptr<int> valuePtr(new int(15));
        void * args = malloc(sizeof(new_socket) + sizeof(vent_ID));
        memcpy(args, &new_socket, sizeof(int));
        memcpy((char *)args + sizeof(int), &vent_ID, sizeof(int));

        pthread_create(&vent_recv_threads[vent_ID], NULL, recv_packets, args);

        vent_ID++;

        // free(args);
        // pthread_join(vent_recv_threads[vent_ID], NULL);

        //TODO: Collect this socket
    }
    return NULL;
}

int main(void){
    pthread_t setup_thread;
    pthread_t consumer_thread;
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    cout << "create setup thread " << endl;
    pthread_create(&setup_thread, NULL, connection_setup, NULL);


    //Cleanup
    pthread_join(setup_thread, NULL);
    // pthread_join(consumer_thread, NULL);

    //collect all recv threads

    return 0;
}