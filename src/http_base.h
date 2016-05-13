#pragma once

#include <cstring>
#include <ctime>
#include <cctype>

#include <string>
#include <sstream>
#include <map>

#include <WinSock2.h>
#include <windows.h>

#include "file_system.h"

namespace taoweb {
    class win_sock
    {
    public:
        win_sock() {
            WSADATA _wsa;
            ::WSAStartup(MAKEWORD(2, 2), &_wsa);
        }

        ~win_sock() {
            ::WSACleanup();
        }
    };

    void init_winsock();

    struct client_t {
        in_addr     addr;
        uint16_t    port;
        SOCKET      fd;
    };

    class socket_server_t
    {
    public:
        socket_server_t(const char* addr, uint16_t port, uint16_t backlog = 128) {
            _addr.S_un.S_addr = ::inet_addr(addr);
            _port = port;
            _backlog = backlog;
        }

        ~socket_server_t() {

        }

    public:
        void start();

        bool accept(client_t* c);

    protected:
        SOCKET      _fd;
    protected:
        in_addr     _addr;
        uint16_t    _port;
        uint16_t    _backlog;
    };

    class http_header_t
    {
        using string = std::string;

        struct string_nocase_compare
        {
            bool operator()(const string& lhs, const string& rhs) {
                return _stricmp(lhs.c_str(), rhs.c_str()) < 0;
            }
        };

        typedef std::map<string, string, string_nocase_compare> strstrimap;

    public:
        http_header_t() {}
        ~http_header_t() {}

    public:
        http_header_t& put(const string& name, const string& value);
        http_header_t& put_status(const string& code, const string& reason);

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

    // �����ļ���չ����ȡ MIME ���ͣ�ȡ����ʱ���� def
    std::string mime(const std::string& name, const char* def = "application/octet-stream");

    // ���� HTTP Ҫ��� GMT ��ʽ��ǰʱ��
    std::string gmtime();
}
