//
// Created by 陆清 on 2/23/20.
//

#ifndef H2D1_CACHE_H
#define H2D1_CACHE_H
#include <iostream>
#include <fstream>
#include <sstream>
#include "Request.h"
#include "Response.h"
#include "ServerSocket.h"
#include "ClientSocket.h"
#include <unordered_map>
#include "list"
#include "Logger.h"
#include "PostHandler.h"

class cache{
    class node{
    public:
        std::string cache_url;
        Response cache_response;

    public:
        node(const std::string k, const Response v): cache_url(k), cache_response(v){}
    };
private:
    std::mutex mtx_cache;//std::mutex to protect cache
    std::list<node> cache_list;
    std::unordered_map<std::string, std::list<node>::iterator> cache_map;
    size_t capacity = 2000;
    ServerSocket & serverSocket;
public:
    // connect_pair_t & connectPair;
    cache(const int & c, ServerSocket & serverSocket) : capacity(c), serverSocket(serverSocket) {}
    // virtual connect_pair_t & getConnectPair() {return this->getConnectPair;}
    virtual void check_cache(Request & request, int id, Logger & logger, connect_pair_t & connectPair);
    virtual void put_cache(const std::string key, Response value, const datetime_zone_t time);
    virtual bool getResponseByUrl(Response & response_in_cache, std::string & key);
    virtual int revalidate_header(Request & request, Response & response, int ID, Logger & logger, connect_pair_t & connectPair);
    virtual void response_helper(Request & request, int id, Logger & file, connect_pair_t & connectPair);
    //virtual void reconstructRequest(Request& request, Response& response);
};

bool cache::getResponseByUrl(Response &response_in_cache, std::string &key){
    std::lock_guard<std::mutex> guard(mtx_cache);
    //Response response(id);
    if(cache_map.find(key)!=cache_map.end()){
        response_in_cache = cache_map[key]->cache_response;
        cache_map[key]->cache_response.iscached=true;

        cache_list.erase(cache_map[key]);
        cache_list.push_front({key,response_in_cache});
        cache_map[key] = cache_list.begin();

        return true;
    } 
    
    return false;
}

void cache::put_cache(const std::string key, Response value, const datetime_zone_t r_store_time) {
    std::lock_guard<std::mutex> guard(mtx_cache);
    //if map(cache) has key
    if (cache_map.find(key) != cache_map.end())
        cache_list.erase(cache_map[key]);
    else if (cache_list.size() >= capacity) {
        cache_map.erase(cache_list.back().cache_url);
        cache_list.pop_back();
    }
    cache_list.push_front({ key, value });
    cache_map[key] = cache_list.begin();
    cache_map[key]->cache_response.getStoredTime()=r_store_time;//TODO:maybe have mistatke

}

// if (n==0) {
//     //304
// }
// else if (n==1) {
//     //no-store
// }
// else if (n==2) {
//     //put in cache
// }
// else if (n==3) {
//     //no cache control tag in new response
// }
// else if (n==4) {
//     //recieved nothing from server in validation
// }
// else if (n==5) {
//     //not 200
// }
int cache::revalidate_header(Request & request, Response & response, int id, Logger & file, connect_pair_t & connectPair) {
    mapped_field_t response_header_map_prev=response.getHeaderFields();
    auto it_etag = response_header_map_prev.find("Etag");
    std::string response_etag_material;
    if (it_etag!=response_header_map_prev.end()){
        //TODO:reconstruct request header with etag
        request.getHeaderFields()["If-None-Match"] = it_etag->second;
        auto it_last_modified = response_header_map_prev.find("Last-Modified");
        if (it_last_modified != response_header_map_prev.end()) {
            request.getHeaderFields()["If-Modified-Since"] = it_last_modified->second;
        }
    }
    else{
        auto it_last_modified = response_header_map_prev.find("Last-Modified");
        if (it_last_modified != response_header_map_prev.end()) {
            request.getHeaderFields()["If-Modified-Since"] = it_last_modified->second;
        }
    }
    //connect with web server
    ClientSocket clientSocket(request.getHostName(), request.getPort());
    std::vector<char> requestMsg = request.reconstruct();
    file.sendingRequest(request);
    clientSocket.socketSend(requestMsg);//send to server

    std::vector<char> responseMsgNew;
    Response NewResponse(request.getId());
    clientSocket.socketRecv(responseMsgNew, NewResponse);//recv server new response
    if (responseMsgNew.size() == 0) {
        closeSockfd(connectPair.first);
        return 4;
    } // commented when responseMst resized in client socketRecv
    cleanVectorChar(NewResponse.getContent());
    NewResponse.getContent() = obtainContent(responseMsgNew);
    file.receivedResponse(NewResponse, request, id);//TODO:Maybe has mistake

    //response status code is 304 then just return old response and update cache
    if(strcmp(NewResponse.getStatusCode().data(), "304") == 0){
        std::string url = request.getUri().data();
        datetime_zone_t currTime = getCurrentTime();
        put_cache(url, response, currTime);//put old response
        std::vector<char> reconRespMsg = response.reconstruct();
        serverSocket.socketSend(reconRespMsg, connectPair);//send to browser
        //TODO:MAYBE NOT CORRECT
        file.sendingResponse(response, (long) id);
        return 0;
    }

    // not 304, update cache
    // check is 200?
    if (strcmp(NewResponse.getStatusCode().data(), "200") != 0) {
        // not cacheable
        std::vector<char> reconRespMsg = NewResponse.reconstruct();
        // file.receivedResponse(NewResponse, request, id);
        // file.notCacheable(id, "not \"200\"");
        serverSocket.socketSend(reconRespMsg, connectPair);
        file.sendingResponse(NewResponse, id);
        return 5;  // no need to check cache
    }

    mapped_field_t response_header_map_new = response.getHeaderFields();
    auto it_response_new = response_header_map_new.find("Cache-Control");
    // std::string response_cache_control_material;
    if (it_response_new != response_header_map_new.end()) { // new response has cache-control
        std::string response_cache_control_material = it_response_new->second.data();
        //if response(server) 200, no store->send to browser
        if ((response_cache_control_material.find("no-store") != std::string::npos || 
        response_cache_control_material.find("private") != std::string::npos)) {
            //将server response 返回 browser,且不放进cache
            std::vector<char> reconRespMsg = NewResponse.reconstruct();
            // file.receivedResponse(NewResponse, request, id);
            file.notCacheable(id, "\"no-store\" or \"private\"");
            serverSocket.socketSend(reconRespMsg, connectPair);//send to browser
            file.sendingResponse(NewResponse, (long) id);
            return 1;
        }
        else { // first time to put into cache?
            std::string url = request.getUri().data();
            datetime_zone_t currTime = getCurrentTime();
            
            put_cache(url, NewResponse, currTime);
            // file.receivedResponse(response, request, id);
            //if new response's head has no-cache
            if(response_cache_control_material.find("no-cache")!=std::string::npos ||
               response_cache_control_material.find("max-age=0") != std::string::npos){
                file.cachedRequiresRevalidation(id);
            }
            else if (response_cache_control_material.find("max-age") != std::string::npos) {//update expire time?
                size_t pos = response_cache_control_material.find("max-age=");
                pos += 8;
                size_t pos1 = response_cache_control_material.find(",", pos);
                std::string age_content = response_cache_control_material.substr(pos, pos1 - pos);
                int age = stoi(age_content);
                time_t res = std::mktime(&currTime.first) + (time_t) age;
                file.cachedExpiresAtX(id, res);
            }
            else {
                file.cachedRequiresRevalidation(id);
            }
            //send to browser
            std::vector<char> reconRespMsg = NewResponse.reconstruct();
            serverSocket.socketSend(reconRespMsg, connectPair);
            file.sendingResponse(NewResponse, (long) id);
            return 2;
        }
    }
    else {
        //no cache control in new response
        std::string url = request.getUri().data();
        datetime_zone_t currTime = getCurrentTime();
        time_t expire_time;

        put_cache(url, NewResponse, currTime);
        auto it_response_expires = response_header_map_new.find("Expires");
        // std::vector<char> response_expire_material;
        if(it_response_expires != response_header_map_new.end()) {//header hase expires
            std::vector<char> response_expire_material = it_response_expires->second;//expire time
            // int expire=stoi(response_expire_material);
            std::tm expire = getDatetimeAndZone(response_expire_material).first;
            expire_time = std::mktime(&currTime.first) + std::mktime(&expire);
            file.cachedExpiresAtX(id, expire_time);
        }
        else{
            //TODO:no expire tag
            time_t store_time = std::mktime(&currTime.first);
            auto it_response_last_modified = response_header_map_new.find("Last-Modified");
            std::vector<char> response_last_modified_material;
            if (it_response_last_modified != response_header_map_new.end()) { // cached response has last-modified
                std::tm datetm = getDatetimeAndZone(response.getDatetimeVectorChar()).first;
                time_t date = std::mktime(&datetm);
                std::tm last_modified_tm = getDatetimeAndZone(it_response_last_modified->second).first;
                time_t last_modified = std::mktime(&last_modified_tm);
                expire_time = store_time + (date - last_modified) * 0.1;
                
                // std::cout << "storetime " << asctime(gmtime(&store_time));
            }
            else { // cached response no has last-modified
                expire_time = store_time + (time_t) 36000;
            }
            file.cachedExpiresAtX(id, expire_time);
        }

        std::vector<char> reconRespMsg = NewResponse.reconstruct();
        // file.receivedResponse(NewResponse, request, id);
        serverSocket.socketSend(reconRespMsg, connectPair);
        file.sendingResponse(NewResponse, (long) id);
        return 3;
    }
}

void cache::response_helper(Request & request, int id, Logger & file, connect_pair_t & connectPair){
    std::string url = request.getUri().data();
    // id=request.getId();
    datetime_zone_t currTime = getCurrentTime();
    Response response(request.getId());

    if (!getResponseByUrl(response, url)) { // cache_list does not contain what was requested (not in cache)
        //request uri not stored in cache
        file.notInCache(id);
        //connect to server and get response then store in cache
        ClientSocket clientSocket(request.getHostName(), request.getPort());
        std::vector<char> requestMsg = request.reconstruct();
        file.sendingRequest(request);
        clientSocket.socketSend(requestMsg);//send to server

        std::vector<char> responseMsg;
        // Response response(request.getId());
        response.clearResponse();
        clientSocket.socketRecv(responseMsg, response);//recv server response
        if (responseMsg.size() == 0) {
            closeSockfd(connectPair.first);
            return;
        } // commented when responseMst resized in client socketRecv
        cleanVectorChar(response.getContent());
        response.getContent() = obtainContent(responseMsg);//get response content

        if (strcmp(response.getStatusCode().data(), "200") != 0) {
            // not cacheable
            std::vector<char> reconRespMsg = response.reconstruct();
            file.receivedResponse(response, request, id);
            // file.notCacheable(id, "not \"200\"");
            serverSocket.socketSend(reconRespMsg, connectPair);
            file.sendingResponse(response, id);
            return;  // no need to check cache
        }

        // check cache control material for response
        mapped_field_t response_header_map = response.getHeaderFields();
        auto it_response = response_header_map.find("Cache-Control");
        // std::string response_cache_control_material;
        if(it_response!=response_header_map.end()) {// response has cache-control
            std::string response_cache_control_material = it_response->second.data();
            //if response(server) 200 && (no-store || private) -> not cacheable -> send response to browser directly
            if ((response_cache_control_material.find("no-store") != std::string::npos || 
            response_cache_control_material.find("private")!=std::string::npos)) {
                //将server response 返回 browser,且不放进cache
                std::vector<char> reconRespMsg = response.reconstruct();
                file.receivedResponse(response, request, id);
                file.notCacheable(id, "\"no-store\" or \"private\"");
                serverSocket.socketSend(reconRespMsg, connectPair);//send to browser
                file.sendingResponse(response, (long) id);
                return;
            }
            else {
                //put response(from server) to cache
                put_cache(url, response, currTime);
                file.receivedResponse(response, request, id);
                //if new response's head has no-cache
                if(response_cache_control_material.find("no-cache") != std::string::npos ||
                response_cache_control_material.find("max-age=0") != std::string::npos){
                    file.cachedRequiresRevalidation(id);
                }
                else if (response_cache_control_material.find("max-age") != std::string::npos) {//update expire time?
                    size_t pos = response_cache_control_material.find("max-age=");
                    pos += 8;
                    size_t pos1 = response_cache_control_material.find(",", pos);
                    std::string age_content = response_cache_control_material.substr(pos, pos1 - pos);
                    int age = stoi(age_content);
                    time_t res = std::mktime(&currTime.first) + (time_t) age;
                    file.cachedExpiresAtX(id, res);
                }
                else {
                    file.cachedRequiresRevalidation(id);
                }
            }
        }
        else {
            //put response(from server) to cache
            put_cache(url, response, currTime);
            file.receivedResponse(response, request, id);

            time_t expire_time;
            auto it_response_expires = response_header_map.find("Expires");
            // std::vector<char> response_expire_material;
            if(it_response_expires!=response_header_map.end()) {//header hase expires
                std::vector<char> response_expire_material = it_response_expires->second;//expire time
                // int expire=stoi(response_expire_material);
                std::tm expire = getDatetimeAndZone(response_expire_material).first;
                expire_time = std::mktime(&currTime.first) + std::mktime(&expire);
                file.cachedExpiresAtX(id, expire_time);
            }
            else{
                //TODO:no expire tag
                time_t store_time = std::mktime(&currTime.first);
                auto it_response_last_modified = response_header_map.find("Last-Modified");
                // std::vector<char> response_last_modified_material;
                if (it_response_last_modified != response_header_map.end()) { // cached response has last-modified
                    std::tm datetm = getDatetimeAndZone(response.getDatetimeVectorChar()).first;
                    time_t date = std::mktime(&datetm);
                    std::tm last_modified_tm = getDatetimeAndZone(it_response_last_modified->second).first;
                    time_t last_modified = std::mktime(&last_modified_tm);
                    expire_time = store_time + (date - last_modified) * 0.1;
                    
                    // std::cout << "storetime " << asctime(gmtime(&store_time));
                }
                else { // cached response no has last-modified
                    expire_time = store_time + (time_t) 36000;
                }
                file.cachedExpiresAtX(id, expire_time);
            }
        }

        // cached, but expires at 
        // cached, requires revalidation

        //send response from server to client;
        std::vector<char> reconRespMsg = response.reconstruct();
        serverSocket.socketSend(reconRespMsg, connectPair);
        file.sendingResponse(response, (long) id);

    } 
    else {// cache_list contains what was requested (in cache), which was stored in response
        //request store in cache and response has cache control?
        mapped_field_t request_header_map = request.getHeaderFields();
        auto it_request = request_header_map.find("Cache-Control");
        std::string request_cache_control_material;
        if (it_request != request_header_map.end()) {
            request_cache_control_material = it_request->second.data();
            if (request_cache_control_material.find("no-cache") != std::string::npos ||
            request_cache_control_material.find("max-age=0") != std::string::npos) {
                file.inCacheReqiresValidation(id);
                revalidate_header(request, response, id, file, connectPair);
                return; // no need to look at response headers
            }
        }
        // else { // no cache-control, needs revalidation
        //     file.inCacheReqiresValidation(id);
        //     revalidate_header(request, response, id, file, connectPair);
        //     return; // no need to look at response headers
        // }

        //look for cache-control in response
        mapped_field_t response_header_map = response.getHeaderFields();
        auto it_response = response_header_map.find("Cache-Control");
        // std::string response_cache_control_material;
        if (it_response != response_header_map.end()) { // cache-control field exists in cached response
            // std::cout<<"no cache lalalla"<<std::endl;
            std::string response_cache_control_material = it_response->second.data();
            //find no-cache or max-age=0 tag in cache
            if (response_cache_control_material.find("no-cache") != std::string::npos ||
                response_cache_control_material.find("max-age=0") != std::string::npos) {
                // log for no cache
                file.inCacheReqiresValidation(id);
                revalidate_header(request, response, id, file, connectPair);
            }
            //find max-age in cache
            else if (response_cache_control_material.find("max-age") != std::string::npos) {
                size_t pos = response_cache_control_material.find("max-age=");
                pos += 8;
                size_t pos1 = response_cache_control_material.find(",", pos);
                std::string age_content = response_cache_control_material.substr(pos, pos1 - pos);
                int age = stoi(age_content);
                //TODO:check whether fresh
                datetime_zone_t store_time = response.getStoredTime();
                time_t expiretime = std::mktime(&store_time.first) + (time_t) age;
                if (expiretime > std::mktime(&currTime.first)) { // fresh
                    // log for valid
                    file.inCacheValid(id);
                    std::vector<char> reconRespMsg =response.reconstruct();
                    serverSocket.socketSend(reconRespMsg, connectPair);//send to browser
                    //TODO:MAYBE NOT CORRECT
                    file.sendingResponse(response, (long) id);
                    return;
                }
                else { // not fresh
                    // log for re-validation
                    file.inCacheReqiresValidation(id);
                    revalidate_header(request, response, id, file, connectPair);
                }
            }
            else {
                file.inCacheReqiresValidation(id);
                revalidate_header(request, response, id, file, connectPair);
            }
        }
        else {  // cache-control field not exists in cached response
            //check the expires tag
            time_t expire_time;
            auto it_response_expires = response_header_map.find("Expires");
            // std::vector<char> response_expire_material;
            if (it_response_expires != response_header_map.end()) {// cached response header has expires
                std::vector<char> response_expire_material = it_response_expires->second;//expire time
                // int expire=stoi(response_expire_material);
                std::tm expire = getDatetimeAndZone(response_expire_material).first;
                expire_time = std::mktime(&currTime.first) + std::mktime(&expire);
            }
            else {// cached response header does not have expires
                //TODO:no expire tag
                time_t store_time = std::mktime(&response.getStoredTime().first);
                auto it_response_last_modified = response_header_map.find("Last-Modified");
                std::vector<char> response_last_modified_material;
                if (it_response_last_modified != response_header_map.end()) { // cached response has last-modified
                    std::tm datetm = getDatetimeAndZone(response.getDatetimeVectorChar()).first;
                    time_t date = std::mktime(&datetm);
                    std::tm last_modified_tm = getDatetimeAndZone(it_response_last_modified->second).first;
                    time_t last_modified = std::mktime(&last_modified_tm);
                    expire_time = store_time + (date - last_modified) * 0.1;
                }
                else { // cached response no has last-modified
                    expire_time = store_time + (time_t) 36000;
                }
            }

            if (expire_time > std::mktime(&currTime.first)) { // fresh cached response
                file.inCacheValid(id);
                std::vector<char> reconrespMsg = response.reconstruct();
                serverSocket.socketSend(reconrespMsg, connectPair);
                file.sendingResponse(response, (long) id);
            }
            else {
                file.inCacheExpiredAtX(id, expire_time);
                revalidate_header(request, response, id, file,connectPair);
            }
        }
    }
}

void cache::check_cache(Request & request, int id, Logger & file, connect_pair_t & connectPair) {
    //1. first check request header
    std::unordered_map<std::string, std::vector<char> > request_header_map = request.getHeaderFields();
    auto it_request = request_header_map.find("Cache-Control");
    std::string request_cache_control_material;
    if (it_request != request_header_map.end()) {
        request_cache_control_material = it_request->second.data();
        // no-store or private?
        if (request_cache_control_material.find("no-store") != std::string::npos ||
        request_cache_control_material.find("private") != std::string::npos) {
            file.notInCache(id);

            ClientSocket clientSocket(request.getHostName(), request.getPort());
            std::vector<char> requestMsg = request.reconstruct();
            file.sendingRequest(request);
            clientSocket.socketSend(requestMsg);

            std::vector<char> responseMsg;
            Response response(request.getId());
            clientSocket.socketRecv(responseMsg, response);
            if (responseMsg.size() == 0) {
                closeSockfd(connectPair.first);
                return;
            } // commented when responseMst resized in client socketRecv
            cleanVectorChar(response.getContent());
            response.getContent() = obtainContent(responseMsg);
            
            file.receivedResponse(response, request, id);
            file.notCacheable(id, "\"no-store\" or \"private\"");
            std::vector<char> reconRespMsg = response.reconstruct();

            serverSocket.socketSend(reconRespMsg, connectPair);

            file.sendingResponse(response);
            return;
        }
    }
    response_helper(request, id, file, connectPair);
}

#endif //H2D1_CACHE_H
