#pragma once

#include <cstring>

#include <string>

namespace taoweb {
    namespace uri_helper {
        /*
          A-Z a-z 0-9
          - . _ ~ : / ? # [ ] @ ! $ & ' ( ) * + , ; =
          no space
        */
        bool decode_uri(const std::string& uri, std::string* result){
            if(uri.size() == 0 || !result) return false;

            auto p = uri.c_str();
            auto q = p + uri.size();

            for(; p < q;){
                auto c = *p;
                if(c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || strchr("-._~:/?#[]@!$&'()*+,;=", c)){
                    *result += c;
                    p++;
                }
                else if(c == '%'){
                    if(p + 1 < q && p[1] == '%'){
                        *result += '%';
                        p++;
                    }

                    if(p + 2 < q){
                        unsigned char v = 0;
                        p++;
                        if(*p >= '0' && *p <= '9') v = *p - '0';
                        else if(*p >= 'A' && *p <= 'F') v = *p - 'A' + 10;
                        else return false;
                        p++;
                        v = v * 16;
                        if(*p >= '0' && *p <= '9') v += *p - '0';
                        else if(*p >= 'A' && *p <= 'F') v += *p - 'A' + 10;
                        else return false;

                        p++;
                        *result += v;
                    }
                    else {
                        return false;
                    }
                }
                else {
                    return false;
                }
            }
            return true;
        }
    }
}
