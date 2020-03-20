#ifndef __RESPONSE_H__
#define __RESPONSE_H__

#include "utils.h"
#include "HttpParser.h"
// #include <vector>
#include <algorithm>
#include <cassert>

class Response : public HttpParser {
    public:
    Response(u_long id);
    virtual void clearResponse();
    virtual bool parse(std::vector<char> & msg);
    virtual std::vector<char> & getStatusCode() {return this->statusCode;}
    virtual std::vector<char> & getReasonPhrase() {return this->reasonPhrase;}
    virtual std::vector<char> & getDatetimeVectorChar() {return this->datetime;}
    virtual datetime_zone_t & getStoredTime() {return this->storedTime;}
    virtual void updateStoredTime() {this->storedTime = getCurrentTime();}
    // virtual std::vector<char> & getTimeZone() {return this->timeZone;}
    virtual std::vector<char> reconstructLinedHeaders();
    // virtual std::vector<char> reconstructContent();  // content
    bool iscached;
    
    protected:
    std::vector<char> statusCode;
    std::vector<char> reasonPhrase;
    std::vector<char> datetime;
    datetime_zone_t storedTime;
    // std::vector<char> timeZone;
    virtual bool parseFirstLineDetails();
    virtual bool parseDateTime(std::vector<char> & msg);
};

Response::Response(u_long id) : HttpParser(id), iscached(false), statusCode(), reasonPhrase(), datetime(), \
storedTime() {
    this->builtInHeaders.insert("Date");
}

bool Response::parse(std::vector<char> & msg) {
    msg.push_back('\0');
    try {
        if (this->firstLine.size() == 0) {
            this->parseFirstLine(msg);
        }
        if (this->statusCode.size() == 0) { // which means protocal and reason phrase are also empty
            this->parseFirstLineDetails();
        }
        if (this->header.size() == 0) {
            this->parseHeader(msg);
        }
        if (this->headerFields.size() == 0) {
            this->parseHeaderFields();
        }
        if (this->contentLength < 0) {
            this->parseContentLength(msg);
        }
        if (this->datetime.size() == 0) {
            this->parseDateTime(msg);
        }
        //
        if (this->transferEncoding.size() == 0) {
            this->parseTransferEncoding(msg);
        }
        // if (this->content.size() == 0) {
        //     this->parseContent(msg);
        // }
        if (this->checkIsCompleted(msg)) {
            msg.pop_back();
            return true;
        } 
    }
    catch (std::invalid_argument & e) {
        msg.pop_back();
        throw std::invalid_argument(e.what());
    }
    msg.pop_back();
    return true;
}

void Response::clearResponse() {
    this->clear();
    cleanVectorChar(this->statusCode);
    cleanVectorChar(this->reasonPhrase);
    cleanVectorChar(this->datetime);
    datetime_zone_t new_dtz;
    this->storedTime = new_dtz;
    // cleanVectorChar(this->timeZone);
    this->iscached = false;
}

std::vector<char> Response::reconstructLinedHeaders() {
    // // First line
    // std::vector<char> firstLine = response.getFirstLine();
    // // Date
    // std::vector<char> datetimeVectorChar = response.getDatetimeVectorChar();
    // std::vector<char> timeZone = response.getTimeZone();
    // // Content-Length
    // int contentLength = response.getContentLength();
    // // Transfer-Encoding
    // std::vector<char> transferEncoding = response.getTransferEncoding();
    // // Header fields
    // mapped_field_t headerFields = response.getHeaderFields();
    
    std::vector<char> line;
    if (this->protocal.size() > 0 && this->statusCode.size() > 0 && 
    this->reasonPhrase.size() > 0) {
        appendTwoVectorChars(line, this->protocal);
        appendCstrToVectorChar(line, " ");
        appendTwoVectorChars(line, this->statusCode);
        appendCstrToVectorChar(line, " ");
        appendTwoVectorChars(line, this->reasonPhrase);
    }
    this->firstLine = line;

    std::vector<char> hdr;
    if (this->datetime.size() > 0) {
        appendCstrToVectorChar(hdr, "Date: ");
        appendTwoVectorChars(hdr, this->datetime);
        // if (this->timeZone.size() > 0) {
        //     appendCstrToVectorChar(recon, " ");
        //     appendCstrToVectorChar(recon, this->timeZone.data());
        // }
        appendCstrToVectorChar(hdr, "\r\n");
    }
    if (this->contentLength > -1) {
        appendCstrToVectorChar(hdr, "Content-Length: ");
        std::stringstream cl;
        cl << this->contentLength;
        appendCstrToVectorChar(hdr, cl.str().c_str());
        appendCstrToVectorChar(hdr, "\r\n");
    }
    if (this->transferEncoding.size() > 0) {
        appendCstrToVectorChar(hdr, "Transfer-Encoding: ");
        appendTwoVectorChars(hdr, this->transferEncoding);
        appendCstrToVectorChar(hdr, "\r\n");
    }
    if (this->headerFields.size() > 0) {
        std::stringstream hd;
        for (mapped_field_t::iterator it = this->headerFields.begin();
        it != this->headerFields.end(); ++it) {
            hd << it->first << ": " << it->second.data() << "\r\n";
        }
        appendCstrToVectorChar(hdr, hd.str().c_str());
    }
    appendCstrToVectorChar(hdr, "\r\n");
    this->header = hdr;

    std::vector<char> recon;
    appendTwoVectorChars(recon, line);
    appendCstrToVectorChar(recon, "\r\n");
    appendTwoVectorChars(recon, hdr);

    return recon;
}

bool Response::parseFirstLineDetails() {
    if (this->firstLine.size() == 0) {
        return false;
    }
    const char * pcur = this->firstLine.data();
    // Obtain protocal
    std::vector<char> tempProtocal;
    while (*pcur != ' ' && *pcur != '\0') {
        tempProtocal.push_back(*pcur);
        pcur = pcur + 1;
    }
    if (*pcur == '\0') {
        return false;
    }
    skipSpace(&pcur);
    // Obtain status code
    std::vector<char> tempStatusCode;
    while (*pcur != ' ' && *pcur != '\0') {
        tempStatusCode.push_back(*pcur);
        pcur = pcur + 1;
    }
    if (*pcur == '\0') {
        return false;
    }
    skipSpace(&pcur);
    // Obtain reason phrase
    std::vector<char> tempreasonPhrase;
    while (*pcur != '\0') {
        tempreasonPhrase.push_back(*pcur);
        pcur = pcur + 1;
    }
    
    if (tempProtocal.size() > 0) {
        tempProtocal.push_back('\0');
        this->protocal = tempProtocal;
    }
    if (tempStatusCode.size() > 0) {
        tempStatusCode.push_back('\0');
        this->statusCode = tempStatusCode;
    }
    if (tempreasonPhrase.size() > 0) {
        tempreasonPhrase.push_back('\0');
        this->reasonPhrase = tempreasonPhrase;
    }

    return true;
}

bool Response::parseDateTime(std::vector<char> & msg) {
    const char * pch = strstr(msg.data(), "Date:");
    if (pch == NULL) {
        return false;
    }
    pch = pch + strlen("Date:");
    skipSpace(&pch);
    std::vector<char> tempDate;
    while (*pch != '\r' && *pch != '\0') {
        tempDate.push_back(*pch);
        pch = pch + 1;
    }
    if (*pch == '\0') {
        return false;
    }
    if (tempDate.size() > 0) {
        tempDate.push_back('\0');
        this->datetime = tempDate;
        return true;
    }
    std::cerr << "cannot parse datetime!\n";
    return false;
}

#endif