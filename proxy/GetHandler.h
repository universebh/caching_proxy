#ifndef __GETHANDLER_H__
#define __GETHANDLER_H__

#include "ServerSocket.h"
#include "ClientSocket.h"
#include "Logger.h"
#include "Cache.h"

bool handleGet(cache & ch, Logger & logger, Request & request, 
ServerSocket & serverSocket, connect_pair_t & connectPair) {
    ch.check_cache(request, request.getId(), logger, connectPair);
    
    // ClientSocket clientSocket(request.getHostName(), request.getPort());
    // std::vector<char> requestMsg = request.reconstruct();
    // logger.sendingRequest(request);
    // clientSocket.socketSend(requestMsg);

    // std::vector<char> responseMsg;
    // Response response(request.getId());
    // clientSocket.socketRecv(responseMsg, response);

    // if (responseMsg.size() == 0) {
    //     closeSockfd(connectPair.first);
    //     return false;
    // } // commented when responseMst resized in client socketRecv

    // cleanVectorChar(response.getContent());
    // response.getContent() = obtainContent(responseMsg);
    
    // logger.receivedResponse(response, request);
    // std::vector<char> reconRespMsg = response.reconstruct();

    // serverSocket.socketSend(reconRespMsg, connectPair);

    // logger.sendingResponse(response);

    return true;
}

#endif