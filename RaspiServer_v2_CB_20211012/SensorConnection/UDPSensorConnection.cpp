#include "UDPSensorConnection.hpp"
#include "Message/MessageFactory.hpp"
#include "Message/MessageHandler.hpp"
extern "C"
{
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
}
#include <vector>
#include <memory>

UDPSensorConnection::UDPSensorConnection(unsigned short port) : port(port),
                                                                isrunning(true)
{
    T = std::thread(&UDPSensorConnection::manageConnection, this);
}
UDPSensorConnection::~UDPSensorConnection()
{
    isrunning = false;
    T.join();
}
void UDPSensorConnection::manageConnection()
{
    int sock = -1;
    
    //set up socket
    while(isrunning)
    {
        if(sock != -1)
        {
            if(close(sock) == -1)
            {
                continue;
            }
            sock = -1;
        }
        sock = socket(PF_INET, SOCK_DGRAM, 0);
        if(sock == -1)
        {
            continue;
        }
        struct sockaddr_in bindaddress = {};
        bindaddress.sin_addr.s_addr = htonl(INADDR_ANY);
        bindaddress.sin_family = AF_INET;
        bindaddress.sin_port = htons(port);
        if(::bind(sock, reinterpret_cast<struct sockaddr*>(&bindaddress), sizeof(struct sockaddr_in)) == -1)
        {
            continue;
        }
        struct timeval socktimeout;
        socktimeout.tv_sec = 5;
        socktimeout.tv_usec = 0;
        if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &socktimeout, sizeof(struct timeval)) == -1)
        {
            continue;
        }
        break;
    }
    std::cout << "Start running on Port " << port << std::endl;
    while(isrunning)
    {
        //receive new data
        struct sockaddr_storage receivedaddress = {};
        socklen_t addresssize = sizeof(struct sockaddr_storage);
        std::vector<char> receivebuffer(65535, 0);
        ssize_t datagramsize = recvfrom(sock, receivebuffer.data(), receivebuffer.size(), 0, reinterpret_cast<struct sockaddr*>(&receivedaddress), &addresssize);
        if(datagramsize > 0)
        {
            std::string data(receivebuffer.data(), datagramsize);
            std::unique_ptr<Message> message = MessageFactory::createMesssage(data);
            if(message)
            {
                std::vector<std::unique_ptr<Message>> messages = MessageHandler::processMessage(std::move(message));
                for(unsigned int i=0; i<messages.size(); i++)
                {
                    //Sent all answer messages
                    std::string answer = messages[i].get()->toMessage();
                    sendto(sock, answer.data(), answer.size(), MSG_DONTWAIT, reinterpret_cast<struct sockaddr*>(&receivedaddress), addresssize);
                }
            }
        }
    }
    if(sock != -1)
    {
        for(unsigned int i=0; i<3; i++)
        {
            if(close(sock) == 0)
            {
                break;
            }
        }
    }
}
