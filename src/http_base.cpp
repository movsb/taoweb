#include <string>
#include <sstream>
#include <algorithm>

#include "http_base.h"
#include "file_system.h"

namespace taoweb {
    void init_winsock() {
        static win_sock _win_sock;
    }

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

    http_header_t& http_header_t::put(const std::string& name, const std::string& value) {
        _headers[name] = value;
        return *this;
    }

    http_header_t& http_header_t::put_status(const string& code, const string& reason) {
        _code = code;
        _reason = reason;
        return *this;
    }

    std::string http_header_t::get(const char* name) const {
        auto it = _headers.find(name);
        if(it != _headers.cend())
            return it->second;
        else
            return "";
    }

    std::string http_header_t::operator[](const char* name) const {
        return get(name);
    }

    void http_header_t::read(const SOCKET& fd) {
        enum class state_t {
            is_return, is_newline, is_2nd_return, is_2nd_newline,
            b_verb, i_verb, a_verb,
            i_uri, a_uri,
            i_query, a_query,
            i_version, a_version,
            i_key, a_key, i_val, a_val, i_colon,
        };

        state_t state = state_t::b_verb;
        bool reusec = false;
        std::string key, val;

        unsigned char c;
        int r;

        while(reusec || (r = ::recv(fd, (char*)&c, 1, 0)) == 1) {
            reusec = false;
            switch(state) {
            case state_t::b_verb:
            case state_t::i_verb:
                if(c >= 'A' && c <= 'Z') {
                    state = state_t::i_verb;
                    _verb += c;
                    continue;
                } else {
                    if(_verb.size()) {
                        state = state_t::a_verb;
                        reusec = true;
                        continue;
                    } else {
                        goto fail;
                    }
                }
                break;
            case state_t::a_verb:
                if(c == ' ' || c == '\t') {
                    continue;
                } else if(c > 32 && c < 128) {
                    state = state_t::i_uri;
                    reusec = true;
                    continue;
                } else {
                    goto fail;
                }
                break;
            case state_t::i_uri:
                if(c > 32 && c < 128) {
                    if(c == '?') {
                        state = state_t::i_query;
                        continue;
                    } else {
                        _uri += c;
                        continue;
                    }
                } else {
                    state = state_t::a_uri;
                    reusec = true;
                    continue;
                }
            case state_t::i_query:
                if(c > 32 && c < 128) {
                    _query += c;
                    continue;
                } else {
                    state = state_t::a_query;
                    reusec = true;
                    continue;
                }
            case state_t::a_query:
            case state_t::a_uri:
                if(c == ' ' || c == '\t') {
                    continue;
                } else {
                    state = state_t::i_version;
                    reusec = true;
                    continue;
                }
            case state_t::i_version:
                if(c > 32 && c < 128) {
                    _ver += c;
                    continue;
                } else {
                    state = state_t::a_version;
                    reusec = true;
                    continue;
                }
            case state_t::a_version:
                if(c == ' ' || c == '\t') {
                    continue;
                } else if(c == '\r') {
                    state = state_t::is_return;
                    continue;
                } else {
                    goto fail;
                }
            case state_t::is_return:
                if(c == '\n') {
                    state = state_t::is_newline;
                    continue;
                } else {
                    goto fail;
                }
            case state_t::is_newline:
                key = "", val = "";
                if(c == '\r') {
                    state = state_t::is_2nd_return;
                    continue;
                } else {
                    state = state_t::i_key;
                    reusec = true;
                    continue;
                }
            case state_t::is_2nd_return:
                if(c == '\n') {
                    state = state_t::is_2nd_newline;
                    reusec = true; // !!
                    continue;
                } else {
                    goto fail;
                }
            case state_t::is_2nd_newline:
                return;
            case state_t::i_key:
                if(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '-' || c == '_') {
                    state = state_t::i_key;
                    key += c;
                    continue;
                } else if(c == ' ') {
                    state = state_t::a_key;
                    continue;
                } else if(c == ':') {
                    state = state_t::i_colon;
                    continue;
                } else {
                    goto fail;
                }
            case state_t::a_key:
                if(c == ' ') {
                    continue;
                } else if(c == ':') {
                    state = state_t::i_colon;
                    continue;
                } else {
                    goto fail;
                }
            case state_t::i_colon:
                if(c == ' ') {
                    continue;
                } else if(c > 32 && c < 128) {
                    state = state_t::i_val;
                    reusec = true;
                    continue;
                } else {
                    goto fail;
                }
            case state_t::i_val:
                if(c >= 32 && c < 128) {
                    val += c;
                    continue;
                } else if(c == '\r') {
                    if(key.size()) {
                        put(key, val);
                    }
                    state = state_t::is_return;
                    continue;
                } else {
                    goto fail;
                }
            default:
                break;
            }
        }

    fail:;
    }

    std::string http_header_t::serialize() const {
        std::ostringstream oss;

        oss << "HTTP/1.1 ";
        oss << _code << " " << _reason;
        oss << "\r\n";

        for(auto& h : _headers) {
            oss << h.first << ": " << h.second << "\r\n";
        }

        oss << "\r\n";

        return std::move(oss.str());
    }

    // --------------------------------------------------------------------------------------------

    bool socket_server_t::accept(client_t* c) {
        SOCKET cfd;
        sockaddr_in addr;
        int len = sizeof(addr);

        cfd = ::accept(_fd, (sockaddr*)&addr, &len);
        if(cfd != -1) {
            c->addr = addr.sin_addr;
            c->port = ntohs(addr.sin_port);
            c->fd = cfd;

            return true;
        } else {
            return false;
        }
    }

    void socket_server_t::start() {
        _fd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(_fd == -1)
            throw "socket() error.";

        sockaddr_in server_addr = {0};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr = _addr;
        server_addr.sin_port = htons(_port);

        if(::bind(_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
            throw "bind() error.";

        if(::listen(_fd, _backlog) == -1)
            throw "listen() error.";
    }

    // --------------------------------------------------------------------------------------------

}
