#ifndef __CLIENTSOCKET_H__
#define __CLIENTSOCKET_H__

#include "Socket.h"
#include "Response.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
// #include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
// #include <sys/socket.h>
// #include <arpa/inet.h>
#include <sys/time.h>
// #include <unistd.h>	 // usleep
#include <exception>
#include <sstream>

class ClientSocket : public Socket {
    public:
    // std::vector<char> & getIpAddr() {return this->ip;}
    std::vector<char> & getHostName() {return this->hostName;}
    std::vector<char> & getPort() {return this->port;}
    int & getWebServerSockfd() {return this->sockfd;}
    ClientSocket(std::vector<char> hostName, std::vector<char> port);
    virtual ~ClientSocket();
    virtual bool socketSend(std::vector<char> & sendMsg);
    virtual bool socketRecv(std::vector<char> & recvMsg, Response & response);
    
    protected:
    // std::vector<char> ip;
    std::vector<char> hostName;
    std::vector<char> port; 
    virtual bool setup();
    virtual void closeSocket();

    private:
    int sockfd;
};

ClientSocket::ClientSocket(std::vector<char> hostName, std::vector<char> port) : \
Socket(), hostName(hostName), port(port), sockfd(-1) {
    if(!this->setup()) {
        std::stringstream ess;
        ess << __func__;
        throw std::invalid_argument(ess.str());
    }
}

ClientSocket::~ClientSocket() {this->closeSocket();}

bool ClientSocket::setup() {
    // std::string addr = this->hostName;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    std::memset(&hints, 0, sizeof hints);  
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(this->hostName.data(), this->port.data(), &hints, &servinfo)) != 0) { // servinfo: linked list
        // std::cerr << "proxy ClientSocket getaddrinfo: " << gai_strerror(rv) << "\n";
        // std::cerr << "hostName: " << this->hostName.data() << "\n";
        // std::cerr << "port: " << this->port.data() << "\n";
        throw std::out_of_range("Error in setup!");
        // return false;
    }

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((this->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            std::perror("proxy ClientSocket: socket");
            continue;
        }
        if (connect(this->sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(this->sockfd);
            std::perror("proxy ClientSocket: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        this->closeSocket();
        std::cerr << "proxy ClientSocket: failed to connect" << "\n";
        std::cerr << "hostName: " << this->hostName.data() << "\n";
        std::cerr << "port: " << this->port.data() << "\n";
        throw std::invalid_argument("Error in setup!");
        // return false;
    }

    inet_ntop(p->ai_family, this->getInAddr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // cstrToVectorChar(this->ip, s);

    freeaddrinfo(servinfo);

    return true;
}

// open a socket to the real server, send request 
// to the server and receive response from it
bool ClientSocket::socketSend(std::vector<char> & sendMsg) {
    if ((send(this->sockfd, sendMsg.data(), sendMsg.size(), 0)) == -1) {
        this->closeSocket();
        std::perror("send");
        throw std::invalid_argument("Error in send!");
        // return false;
    }

    return true;
}

bool ClientSocket::socketRecv(std::vector<char> & recvMsg, Response & response) {
    int numbytes;
    char recvBuf[MAX_DATA_SIZE];

    struct timeval tv;
    tv.tv_sec = CLIENT_RECV_TIME_OUT;
    tv.tv_usec = 0;
    setsockopt(this->sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    while (1) {
        memset(recvBuf, 0, sizeof recvBuf);
        // MSG_DONTWAIT or 0
        if ((numbytes = recv(this->sockfd, recvBuf, MAX_DATA_SIZE, 0)) > 0) {
            // recvMsg.push_back(recvBuf[0]);
            for (int i = 0; i < numbytes; i++) {
                recvMsg.push_back(recvBuf[i]);
                response.parse(recvMsg);
            }
        }
        else {
            // Do something?
            break;
        }
        // if (numbytes < MAX_DATA_SIZE) {
        //     response.getIsCompleted() = true;
        // }
        if (response.getIsCompleted()) {
            break;
        }
    }
    if (recvMsg.size() > 0) {
        recvMsg.push_back('\0');
    }
    return true;
}
// End of citation

void ClientSocket::closeSocket() {
    closeSockfd(this->sockfd);
}

#endif