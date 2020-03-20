#ifndef __UTILS_H__
#define __UTILS_H__

#include <iostream>
#include <sstream>
#include <cctype>
#include <vector>
#include <cstring>
#include <locale>
#include <iomanip>
#include <algorithm>
#include <mutex>
// #include <exception>

std::mutex mtx;

void printALine(size_t size) {
    std::string line;
    for (size_t i = 0; i < size; i++) {
        line += '-';
    }
    std::cout << line << "\n";
}

void skipSpace(const char ** strp) {
    //code cited from homework 081
    while (isspace(**strp)) {
      *strp = *strp + 1;
    }
}

void cleanVectorChar(std::vector<char> & vecChar) {
    while (vecChar.size() > 0) {
        vecChar.pop_back();
    }
}

void cstrToVectorChar(std::vector<char> & vecChar, const char * cstr) {
    cleanVectorChar(vecChar);
    for (size_t i = 0; i < strlen(cstr); i++) {
          vecChar.push_back(cstr[i]);
    }
    if (vecChar.size() > 0) {
        vecChar.push_back('\0');
    }
}

void appendCstrToVectorChar(std::vector<char> & vecChar, const char * cstr) {
    while (vecChar.size() > 0 && vecChar[vecChar.size()-1] == '\0') {
        vecChar.pop_back(); // pop '\0'
    }
    for (size_t i = 0; i < strlen(cstr); i++) {
          vecChar.push_back(cstr[i]);
    }
    if (vecChar.size() > 0) {
        vecChar.push_back('\0');
    }
}

void appendTwoVectorChars(std::vector<char> & des, const std::vector<char> & sou) {
    while (des.size() > 0 && des.back() == '\0') {
        des.pop_back(); // pop '\0'
    }
    for (size_t i = 0; i < sou.size(); i++) {
        des.push_back(sou[i]);
    }
    if (des.size() > 0 && des.back() != '\0') {
        des.push_back('\0');
    }
}

typedef std::pair<std::tm, std::vector<char> > datetime_zone_t;
// datetimeVectorChar: datetime TIME_ZONE
datetime_zone_t getDatetimeAndZone(std::vector<char> datetime) {
    if (datetime.size() == 0) {
        std::stringstream ess;
        ess << "datetime has no content in " << __func__;
        throw std::invalid_argument(ess.str());
    }
    if (datetime[datetime.size()-1] != '\0') {
        datetime.push_back('\0');
    }
    if (strstr(datetime.data(), " ") == NULL) {
        std::stringstream ess;
        ess << __func__ << ": no space found in datetime!";
        throw std::invalid_argument(ess.str());
    }
    datetime.pop_back(); // pop '\0'

    std::vector<char> timeZone;
    while (datetime[datetime.size()-1] != ' ') {
        timeZone.push_back(datetime[datetime.size()-1]);
        datetime.pop_back();
    }
    datetime.pop_back(); // pop ' '
    if (timeZone.size() > 0) {
        std::reverse(timeZone.begin(), timeZone.end());
        timeZone.push_back('\0');
    }
    if (datetime.size() > 0) {
        datetime.push_back('\0');
    }

    std::tm t = {};
    std::istringstream ss(datetime.data());
    // ss.imbue(std::locale("de_DE.utf-8"));
    ss >> std::get_time(&t, "%a, %d %b %Y %H:%M:%S");
    if (ss.fail()) {
        std::stringstream ess;
        ess << "Parse failed on datetime in " << __func__;
        throw std::invalid_argument(ess.str());
    } 

    datetime_zone_t res(t, timeZone);
    return res;
}

datetime_zone_t getCurrentTime() {
    time_t rawtime;
    std::tm * ptm;

    time(&rawtime);
    ptm = gmtime(&rawtime);

    std::vector<char> timeZone;
    cstrToVectorChar(timeZone, "GMT");
    datetime_zone_t datetimeZone(*ptm, timeZone);

    return datetimeZone;
}

std::vector<char> obtainContent(std::vector<char> & msg) {
    std::vector<char> content;
    const char * pcur = strstr(msg.data(), "\r\n\r\n");
    if (pcur == NULL) {
        return content;
    }

    bool sawHead = false;
    for (size_t i = 0; i < msg.size(); i++) {
        if (!sawHead && msg[i] == '\r' && msg[i+1] == '\n' && 
        msg[i+2] == '\r' && msg[i+3] == '\n') {
            sawHead = true;
            i = i + 4;
        }
        if (sawHead) {
            content.push_back(msg[i]);
        }
    }

    return content;
}

#endif