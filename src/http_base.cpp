#include <string>
#include <sstream>
#include <algorithm>
#include <regex>

#include "http_base.h"
#include "file_system.h"

namespace taoweb {

    void init_winsock() {
        static WinSock _WinSock;
    }

    // --------------------------------------------------------------------------------------------

    std::string gmtime() {
        time_t now = time(nullptr);
        tm* gmt = ::gmtime(&now);

        // http://en.cppreference.com/w/c/chrono/strftime
        // e.g.: Sat, 22 Aug 2015 11:48:50 GMT
        //       5+   3+4+   5+   9+       3   = 29
        const char* fmt = "%a, %d %b %Y %H:%M:%S GMT";
        char tstr[30];

        strftime(tstr, sizeof(tstr), fmt, gmt);

        return tstr;
    }

    std::string mime(const std::string& name, const char* def) {
        static struct {
            const char* mime;
            const char* exts;
        } known_mimes[] = {
            // web page
            {"text/html",           "html\x00htm\x00shtml\x00"},
            {"text/css",            "css\x00"},
            {"text/xml",            "xml\x00"},
            {"text/javascript",     "js\x00"},
            {"application/json",    "json\x00"},

            // normal
            {"text/plain",          "txt\x00ini\x00"},

            // source code
            {"text/plain",          "c\x00cc\x00\x00cpp\x00cxx\x00h\x00hpp\x00"},
            {"text/plain",          "php\x00sh\x00"},

            // image / video
            {"image/gif",           "gif\x00"},
            {"image/jpeg",          "jpg\x00jpeg\x00"},
            {"image/png",           "png\x00"},
            {"image/x-icon",        "ico\x00"},
            {"image/x-ms-bmp",      "bmp\x00"},
        };

        std::string result;

        auto ext = std::get<1>(file_system::base_and_ext(name));
        if(!ext.empty()) {
            for(auto& known_mime : known_mimes) {
                const char* p = known_mime.exts;
                while(*p != '\0') {
                    if(_stricmp(ext.c_str() + 1, p) == 0) {
                        result.assign(known_mime.mime);
                        goto _l_exit;
                    }
                        
                    while(*p++) // skip to next
                        ;
                }
            }
        }

    _l_exit:
        if(result.empty())
            result = def;

        return result;
    }

    // --------------------------------------------------------------------------------------------

    HTTPHeader& HTTPHeader::put(const string& name, const string& value) {
        _headers[name] = value;
        return *this;
    }

    HTTPHeader& HTTPHeader::put_status(const string& code, const string& reason) {
        _code = code;
        _reason = reason;
        return *this;
    }

    HTTPHeader::string HTTPHeader::get(const char* name) const {
        auto it = _headers.find(name);
        if(it != _headers.cend())
            return it->second;
        else
            return "";
    }

    HTTPHeader::string HTTPHeader::operator[](const char* name) const {
        return get(name);
    }

    void HTTPHeader::read(const SOCKET& fd) {
        const int buf_size = 4096;
        char buf[buf_size];
        std::string stream;

        /*  GET /index.html HTTP/1.1\r\n
         *  Host: localhost\r\n
         *  User-Agent: Mozilla/5.0 (bla bla)\r\n
         *  \r\n
         *  
         **/

        // 先读取并保存所有的 HTTP 请求头部供后面一次性处理
        for(;;) {
            int n = ::recv(fd, &buf[0], buf_size, 0);
            if(n > 0) {
                stream.append(buf, n);

                // 判断是否已经读完所有的头部（以空行结束，只考虑 \r\n 换行符）
                if(stream.size() > 4 && strcmp(&stream[stream.size() - 4], "\r\n\r\n") == 0) {
                    break;
                }
            }
            else {
                // 简单处理错误
                stream.clear();
                break;
            }
        }

        // 如果有错误。。。
        if(stream.empty()) {
            return;
        }

        try {
            std::smatch matches;
            std::string suffix(stream);

            // 匹配请求方法
            if(std::regex_search(suffix, matches, std::regex(R"(^(\w+)[\t ]+)"))) {
                _verb = matches[1];
                suffix = matches.suffix().str();
            } else {
                throw "缺少请求方法。";
            }

            // 匹配URI和QUERY
            if(std::regex_search(suffix, matches, std::regex(R"(^([^\?\t ]+)(\?[^\t ]+)?)"))) {
                _uri = matches[1];
                auto query = matches[2].str();
                suffix = matches.suffix().str();

                if(!query.empty()) {
                    auto suffix = query.substr(1);

                    for(;;) {
                        if(std::regex_search(suffix, matches, std::regex(R"(^&)"))) {
                            suffix = matches.suffix().str();
                        } else if(std::regex_search(suffix, matches, std::regex(R"(^(\w)(=(\w*)?)?)"))) {
                            auto k = matches[1];
                            auto v = matches[3];
                            _query[k] = v;
                            suffix = matches.suffix().str();
                        } else {
                            break;
                        }
                    }
                }
            } else {
                throw "缺少URI。";
            }

            // 匹配 HTTP 版本
            if(std::regex_search(suffix, matches, std::regex(R"(^[\t ]+(\w+/\d+\.\d+)\r?\n)"))) {
                _ver = matches[1];
                suffix = matches.suffix().str();
            } else {
                throw "缺少HTTP协议。";
            }

            // 匹配 请求字段
            while(std::regex_search(suffix, matches, std::regex(R"(^([-\w]+)[\t ]*:[\t ]*(.*?)[\t ]*\r?\n)"))) {
                auto k = matches[1];
                auto v = matches[2];
                _headers[k] = v;
                suffix = matches.suffix().str();
            }

            // 匹配结尾空行
            if(!std::regex_match(suffix, std::regex(R"(\r?\n)")))
                throw "缺少结束换行符。";
        }
        catch(const char*) {
            _headers.clear();
            return;
        }
    }

    std::string HTTPHeader::serialize() const {
        std::ostringstream oss;

        oss << "HTTP/1.1 " << _code << " " << _reason << "\r\n";

        for(auto& h : _headers) {
            oss << h.first << ": " << h.second << "\r\n";
        }

        oss << "\r\n";

        return std::move(oss.str());
    }

    // --------------------------------------------------------------------------------------------

    bool SocketServer::accept(Client* c) {
        SOCKET cfd;
        sockaddr_in addr;
        int len = sizeof(addr);

        cfd = ::accept(_fd, (sockaddr*)&addr, &len);
        if(cfd != -1) {
            c->addr = addr.sin_addr;
            c->port = ntohs(addr.sin_port);
            c->fd   = cfd;

            return true;
        } else {
            return false;
        }
    }

    void SocketServer::start() {
        _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(_fd == -1)
            throw "socket() error.";

        sockaddr_in server_addr = {0};
        server_addr.sin_family  = AF_INET;
        server_addr.sin_addr    = _addr;
        server_addr.sin_port    = htons(_port);

        if(::bind(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
            throw "bind() error.";

        if(::listen(_fd, _backlog) == -1)
            throw "listen() error.";
    }

    // --------------------------------------------------------------------------------------------

}
