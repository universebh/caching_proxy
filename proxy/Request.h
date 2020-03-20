#ifndef __REQUEST_H__
#define __REQUEST_H__

#include "utils.h"
#include "HttpParser.h"
// #include <iostream>
// #include <vector>
// #include <cstring>
#include <cassert>

class Request : public HttpParser {
    public:
    Request(u_long id);
    virtual void clearRequest();
    virtual bool parse(std::vector<char> & msg);
    virtual std::vector<char> & getUri() {return this->uri;}
    virtual std::vector<char> & getHostName() {return this->hostName;}
    virtual std::vector<char> & getPort() {return this->port;}
    virtual std::vector<char> & getMethod() {return this->method;}
    virtual std::vector<char> reconstructLinedHeaders();
    // virtual std::vector<char> reconstructContent();  // content

    protected:
    std::vector<char> uri;
    std::vector<char> hostName;
    std::vector<char> port;
    std::vector<char> method;
    virtual bool parseFirstLineDetails(); 
    virtual bool parseHostName(std::vector<char> & msg);
    virtual bool parseMethod(std::vector<char> & msg);
};

Request::Request(u_long id) : HttpParser(id), uri(), hostName(), port(), method() {
    this->builtInHeaders.insert("Host");
}

bool Request::parse(std::vector<char> & msg) {
    msg.push_back('\0');
    try {
        if (this->firstLine.size() == 0) {
            this->parseFirstLine(msg);
        }
        if (this->uri.size() == 0) { // which means protocal is also empty
            this->parseFirstLineDetails();
        }
        if (this->header.size() == 0) {
            this->parseHeader(msg);
        }
        if (this->headerFields.size() == 0) {
            this->parseHeaderFields();
        }
        if (this->hostName.size() == 0) {
            this->parseHostName(msg);
        }
        if (this->hostName.size() > 0) {
            assert(this->port.size() > 0);
        }
        if (this->method.size() == 0) {
            this->parseMethod(msg);
        }
        if (this->contentLength < 0) {
            this->parseContentLength(msg);
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

void Request::clearRequest() {
    this->clear();
    cleanVectorChar(this->uri);
    cleanVectorChar(this->hostName);
    cleanVectorChar(this->port);
    cleanVectorChar(this->method);
}

std::vector<char> Request::reconstructLinedHeaders() {
    /*
        First line, Host, header fields
    */
    
    std::vector<char> line;
    if (this->method.size() > 0 && this->uri.size() > 0 && 
    this->protocal.size() > 0) {
        appendCstrToVectorChar(line, this->method.data());
        appendCstrToVectorChar(line, " ");
        appendCstrToVectorChar(line, this->uri.data());
        // if (this->port.size() > 0 && strcmp(this->port.data(), "80") != 0) {
        //     appendCstrToVectorChar(line, ":");
        //     appendCstrToVectorChar(line, this->port.data());
        // }
        appendCstrToVectorChar(line, " ");
        appendCstrToVectorChar(line, this->protocal.data());
    }
    this->firstLine = line;

    std::vector<char> hdr;
    if (this->hostName.size() > 0) {
        appendCstrToVectorChar(hdr, "Host: ");
        appendCstrToVectorChar(hdr, this->hostName.data());
        if (this->port.size() > 0 && strcmp(this->port.data(), "80") != 0) {
            appendCstrToVectorChar(hdr, ":");
            appendCstrToVectorChar(hdr, this->port.data());
        }
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
    appendCstrToVectorChar(recon, line.data());
    appendCstrToVectorChar(recon, "\r\n");
    appendCstrToVectorChar(recon, hdr.data());

    return recon;
}

bool Request::parseFirstLineDetails() {
    if (this->firstLine.size() == 0) {
        return false;
    }
    const char * pcur = this->firstLine.data();
    // Skip method
    while (*pcur != ' ' && *pcur != '\0') {
        pcur = pcur + 1;
    }
    if (*pcur == '\0') {
        return false;
    }
    skipSpace(&pcur);
    // Obtain uri
    std::vector<char> tempUri;
    while (*pcur != ' ' && *pcur != '\0') {
        tempUri.push_back(*pcur);
        pcur = pcur + 1;
    }
    if (*pcur == '\0') {
        return false;
    }
    skipSpace(&pcur);
    // Obtain protocal
    std::vector<char> tempProtocal;
    while (*pcur != '\0') {
        tempProtocal.push_back(*pcur);
        pcur = pcur + 1;
    }

    if (tempUri.size() > 0) {
        tempUri.push_back('\0');
        // if (strstr(tempUri.data(), ":") != NULL) {
        //     tempUri.pop_back();  // pop '\0'
        //     while (tempUri.back() != ':') {
        //         tempUri.pop_back();
        //     }
        //     tempUri.pop_back(); // pop ':'
        //     if (tempUri.size() > 0) {
        //         tempUri.push_back('\0');
        //         this->uri = tempUri;
        //     }
        // }
        // else {
        //     this->uri = tempUri;
        // }
        this->uri = tempUri;
    }

    if (tempProtocal.size() > 0) {
        tempProtocal.push_back('\0');
        this->protocal = tempProtocal;
    }

    return true;
}

bool Request::parseHostName(std::vector<char> & msg) {
    const char *pch = strstr(msg.data(), "Host:");
    if (pch == NULL) {
        return false;
    }
    pch = pch + 5;
    skipSpace(&pch);
    std::vector<char> tempHostName;
    while (*pch != '\r' && *pch != '\0') {
        tempHostName.push_back(*pch);
        pch = pch + 1;
    }
    if (*pch == '\0') {
        return false;
    }

    if (tempHostName.size() > 0) {
        tempHostName.push_back('\0');
        if (strstr(tempHostName.data(), ":") != NULL) {
            tempHostName.pop_back(); // pop '\0'
            while (tempHostName.back() != ':') {
                this->port.push_back(tempHostName[tempHostName.size()-1]);
                tempHostName.pop_back(); 
            }
            tempHostName.pop_back();  // pop ':'
            if (this->port.size() > 0) {
                std::reverse(this->port.begin(), this->port.end());
                this->port.push_back('\0');
            }
            if (tempHostName.size() > 0) {
                tempHostName.push_back('\0');
            }
        }
        else {
            // tempHostName.pop_back(); // pop '\0' // commented if tempHost already pushed '\0'
            cstrToVectorChar(this->port, "80");
        }
        this->hostName = tempHostName;
    }
    
    return true;
}

bool Request::parseMethod(std::vector<char> & msg) {
    if (strstr(msg.data(), "GET") != NULL) {
        cstrToVectorChar(this->method, "GET");
    }
    else if (strstr(msg.data(), "POST") != NULL) {
        cstrToVectorChar(this->method, "POST");
    }
    else if (strstr(msg.data(), "CONNECT") != NULL) {
        cstrToVectorChar(this->method, "CONNECT");
    }

    return true;
}

#endif