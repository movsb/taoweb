#pragma once

#include <cstring>
#include <ctime>
#include <cctype>

#include <string>
#include <sstream>
#include <map>

#include <WinSock2.h>
#include <windows.h>

#include "logger.hpp"
#include "file_system.h"

namespace taoweb {
    class WinSock
    {
    public:
        WinSock() {
            WSADATA _wsa;
            ::WSAStartup(MAKEWORD(2, 2), &_wsa);
        }

        ~WinSock() {
            ::WSACleanup();
        }
    };

    void init_winsock();

    struct Client {
        in_addr     addr;
        uint16_t    port;
        SOCKET      fd;
    };

    class SocketServer
    {
    public:
        SocketServer(const char* addr, uint16_t port, uint16_t backlog = 128) {
            _saddr = addr;
            _addr.S_un.S_addr = ::inet_addr(addr);
            _port = port;
            _backlog = backlog;
        }

        ~SocketServer() {

        }

    public:
        void start();

        bool accept(Client* c);

    protected:
        SOCKET      _fd;
    protected:
        const char* _saddr;
        in_addr     _addr;
        uint16_t    _port;
        uint16_t    _backlog;
    };

    class HTTPHeader
    {
        using string = std::string;

        struct string_nocase_compare
        {
            bool operator()(const string& lhs, const string& rhs) const {
                return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
            }
        };

        typedef std::map<string, string, string_nocase_compare> strstrimap;

    public:
        HTTPHeader() {}
        ~HTTPHeader() {}

    public:
        HTTPHeader& put(const string& name, const string& value);
        HTTPHeader& put_status(const string& code, const string& reason);

        string get(const char* name) const;
        string operator[](const char* name) const;

        string get_verb() const {
            return _verb;
        }

        string get_uri() const {
            return _uri;
        }

        void set_uri(const string& uri) {
            _uri = uri;
        }

        const strstrimap& get_query() const {
            return _query;
        }

        string get_ver() const {
            return _ver;
        }

        bool empty() const {
            return _headers.empty();
        }

        void for_each(std::function<void(const string& name, const string& value)> callback) {
            for(auto& it : _headers) {
                callback(it.first, it.second);
            }
        }

        void read(const SOCKET& fd);

        string serialize() const;

        operator string() const {
            return serialize();
        }

    protected:
        string      _verb;
        string      _uri;
        string      _ver;
        string      _code;
        string      _reason;
        strstrimap  _query;
        strstrimap  _headers;
    };

    // 根据文件扩展名获取 MIME 类型，取不到时返回 def
    std::string mime(const std::string& name, const char* def = "application/octet-stream");

    // 生成 HTTP 要求的 GMT 格式当前时间
    std::string gmtime();
}
