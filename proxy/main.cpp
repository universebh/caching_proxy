#include "utils.h"
#include "ClientSocket.h"
#include "ServerSocket.h"
#include "Request.h"
#include "Response.h"
#include "ConnectTunnel.h"
#include "GetHandler.h"
#include "PostHandler.h"
#include "Logger.h"
#include "Cache.h"
#include <thread>
#include <functional>

void runProxy(cache & ch, const u_long & id, Logger & logger,
ServerSocket & serverSocket, connect_pair_t connectPair) {
    std::vector<char> requestMsg;

    Request request(id);
    if (!serverSocket.socketRecv(requestMsg, connectPair, request)) {
        closeSockfd(connectPair.first);
        return;
    }
    if (requestMsg.size() == 0) {
        closeSockfd(connectPair.first);
        return;
    }

    std::vector<char> hostName = request.getHostName();
    std::vector<char> method = request.getMethod();

    if (hostName.size() == 0) {
        closeSockfd(connectPair.first);
        return;
    }
    cleanVectorChar(request.getContent());
    request.getContent() = obtainContent(requestMsg);
    // std::vector<char> reconReqMsg = request.reconstruct();

    logger.receivedRequest(request, serverSocket.getIpAddr());

    try {
        if (strcmp(method.data(), "CONNECT") == 0) {
            if (!handleConnect(id, logger, request, serverSocket, connectPair)) {
                closeSockfd(connectPair.first);
                return;
            }
        }
        else if (strcmp(method.data(), "GET") == 0) {
            handleGet(ch, logger, request, serverSocket, connectPair);
        }
        else if (strcmp(method.data(), "POST") == 0) {
            handlePost(logger, request, serverSocket, connectPair);
        }
    
        closeSockfd(connectPair.first);
    }
    catch (std::out_of_range) {
        std::string responseString = "HTTP/1.1 404 Not Found";

        std::vector<char> respVectorChar;
        cstrToVectorChar(respVectorChar, (responseString + "\r\n\r\n").c_str());
        serverSocket.socketSend(respVectorChar, connectPair);
        std::stringstream ss;
        ss << id << ": Responding " << "\"" << responseString << "\"" << "\r\n";
        logger.write(ss.str());

        closeSockfd(connectPair.first);
    }
}

int main(int argc, char *argv[]) {
    try {
        u_long id = 0;
        Logger logger;
        ServerSocket serverSocket; // receive request from user's browser
        cache ch(200, serverSocket);
        
        while (1) {
            connect_pair_t connectPair = serverSocket.socketAccept();
            id++;
            std::thread th(runProxy, std::ref(ch), std::ref(id), std::ref(logger),
            std::ref(serverSocket), connectPair);
            th.join();
        }
    }
    catch (std::exception & e) {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
