#ifndef __LOGGER_H__
#define __LOGGER_H__

#include "utils.h"
#include "Request.h"
#include "Response.h"
#include <fstream>

const u_long selectId(const u_long & uid, const long & id) {
    if (id < 0) {
        return uid;
    }
    else {
        u_long ans = (uid > (u_long) id)? uid : (u_long) id;
        return ans;
    }
}

class Logger {
    public:
    Logger();
    virtual void openLogger(bool clear=true);
    virtual void closeLogger();
    virtual void write(std::string msg);
    virtual void receivedRequest(Request & request, const std::vector<char> & ip);
    virtual void sendingRequest(Request & request);
    virtual void receivedResponse(Response & response, Request & request, const long id=-1);
    virtual void sendingResponse(Response & response, const long id=-1);
    virtual void tunnelClosed(const u_long & id);
    virtual void notInCache(const u_long & id);
    virtual void inCacheExpiredAtX(const u_long & id, time_t & expireTime);
    virtual void inCacheReqiresValidation(const u_long & id);
    virtual void inCacheValid(const u_long & id);
    virtual void notCacheable(const u_long & id, const std::string & reason);
    virtual void cachedExpiresAtX(const u_long & id, time_t & expireTime);
    virtual void cachedRequiresRevalidation(const u_long & id);

    protected:
    std::ofstream log;
};

Logger::Logger() {
    this->openLogger(false);
    this->closeLogger();
}

void Logger::openLogger(bool app) {
    try {
        if (app) {
            log.open("./proxy.log", std::ios::app);
        }
        else {
            log.open("./proxy.log");
        }
    }
    catch (std::exception & e) {
        throw std::invalid_argument(e.what());
    }
}

void Logger::closeLogger() {
    try {
        log.close();
    }
    catch (std::exception & e) {
        throw std::invalid_argument(e.what());
    }
}

void Logger::write(std::string msg) {
    std::lock_guard<std::mutex> lock(mtx);
    this->openLogger();
    this->log << msg;
    // std::cout << msg;
    this->closeLogger();
}

void Logger::receivedRequest(Request & request, const std::vector<char> & ip) {
    std::stringstream loggedReq;
    datetime_zone_t datetimeZone = getCurrentTime();
    loggedReq << request.getId() << ": \"" << request.getMethod().data() << " " << \
    request.getUri().data() << " " << request.getProtocal().data() << "\" from " << \
    ip.data() << " @ " << std::put_time(&datetimeZone.first, "%c") << "\r\n";
    this->write(loggedReq.str());
}

void Logger::sendingRequest(Request & request) {
    std::stringstream loggedReq;
    loggedReq << request.getId() << ": Requesting \"" << request.getMethod().data() << " " << \
    request.getUri().data() << " " << request.getProtocal().data() << "\" from " << \
    request.getHostName().data() << "\r\n";
    this->write(loggedReq.str());
}

void Logger::receivedResponse(Response & response, Request & request, const long id) {
    std::stringstream loggedresp;
    loggedresp << selectId(response.getId(), id) << ": Received \"" << response.getProtocal().data() << \
    " " << response.getStatusCode().data() << " " << response.getReasonPhrase().data() << \
    "\"" << " from " << request.getHostName().data() << "\r\n";
    this->write(loggedresp.str());
}

void Logger::sendingResponse(Response & response, const long id) {
    std::stringstream loggedresp;
    loggedresp << selectId(response.getId(), id) << ": Responding \"" << response.getProtocal().data() << \
    " " << response.getStatusCode().data() << " " << response.getReasonPhrase().data() << \
    "\"" << "\r\n";
    this->write(loggedresp.str());
}

void Logger::tunnelClosed(const u_long & id) {
    std::stringstream loggedresp;
    loggedresp << id << ": Tunnel closed\r\n";
    this->write(loggedresp.str());
}

void Logger::notInCache(const u_long & id) {
    std::stringstream ssch;
    ssch << id << ": not in cache\r\n";
    this->write(ssch.str());
}

void Logger::inCacheExpiredAtX(const u_long & id, time_t & expireTime) {
    std::string expire = asctime(gmtime(&expireTime));
    std::stringstream ssch;
    ssch << id << ": in cache, but expired at " << expire;
    this->write(ssch.str());
}

void Logger::inCacheReqiresValidation(const u_long & id) {
    std::stringstream ssch;
    ssch << id << ": in cache, requires validation\r\n";
    this->write(ssch.str());
}

void Logger::inCacheValid(const u_long & id) {
    std::stringstream ssch;
    ssch << id << ": in cache, valid\r\n";
    this->write(ssch.str());
}

void Logger::notCacheable(const u_long & id, const std::string & reason) {
    std::stringstream ssch;
    ssch << id << ": not cacheable because " << reason << "\r\n";
    this->write(ssch.str());
}

void Logger::cachedExpiresAtX(const u_long & id, time_t & expireTime) {
    std::string expire = asctime(gmtime(&expireTime));
    std::stringstream ssch;
    ssch << id << ": cached, expires at " << expire;
    this->write(ssch.str());
}

void Logger::cachedRequiresRevalidation(const u_long & id) {
    std::stringstream ssch;
    ssch << id << ": cached, but requires re-validation\r\n";
    this->write(ssch.str());
}

#endif