#pragma once

#include <string>
#include <vector>
#include <map>
#include <iostream>

#include <WinSock2.h>
#include <windows.h>

#include "file_system.hpp"

#include "socket.hpp"
#include "http_base.hpp"


namespace taoweb {
    namespace http {
        class http_header_t {
        public:
            void read_headers(SOCKET& fd) {
                unsigned char c;
                int r;

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

                while (reusec || (r = ::recv(fd, (char*)&c, 1, 0)) == 1){
                    //if(!reusec) std::cddout << c;
                    reusec = false;
                    switch (state)
                    {
                    case state_t::b_verb:
                    case state_t::i_verb:
                        if (c >= 'A' && c <= 'Z'){
                            state = state_t::i_verb;
                            _verb += c;
                            continue;
                        }
                        else {
                            if (_verb.size()) {
                                state = state_t::a_verb;
                                reusec = true;
                                continue;
                            }
                            else {
                                goto fail;
                            }
                        }
                        break;
                    case state_t::a_verb:
                        if (c == ' ' || c == '\t') {
                            continue;
                        }
                        else if (c > 32 && c < 128) {
                            state = state_t::i_uri;
                            reusec = true;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                        break;
                    case state_t::i_uri:
                        if (c > 32 && c < 128) {
                            if (c == '?') {
                                taoweb::http::decode_uri(_uri, &_uri_decoded);
                                state = state_t::i_query;
                                continue;
                            }
                            else {
                                _uri += c;
                                continue;
                            }
                        }
                        else {
                            taoweb::http::decode_uri(_uri, &_uri_decoded);
                            state = state_t::a_uri;
                            reusec = true;
                            continue;
                        }
                    case state_t::i_query:
                        if (c > 32 && c < 128) {
                            _query += c;
                            continue;
                        }
                        else {
                            state = state_t::a_query;
                            reusec = true;
                            continue;
                        }
                    case state_t::a_query:
                    case state_t::a_uri:
                        if (c == ' ' || c == '\t') {
                            continue;
                        }
                        else {
                            state = state_t::i_version;
                            reusec = true;
                            continue;
                        }
                    case state_t::i_version:
                        if (c > 32 && c < 128) {
                            _version += c;
                            continue;
                        }
                        else {
                            std::cout << _verb << " " << _uri_decoded << " " << _version << std::endl;
                            state = state_t::a_version;
                            reusec = true;
                            continue;
                        }
                    case state_t::a_version:
                        if (c == ' ' || c == '\t'){
                            continue;
                        }
                        else if (c == '\r') {
                            state = state_t::is_return;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::is_return:
                        if (c == '\n') {
                            state = state_t::is_newline;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::is_newline:
                        key = "", val = "";
                        if (c == '\r') {
                            state = state_t::is_2nd_return;
                            continue;
                        }
                        else {
                            state = state_t::i_key;
                            reusec = true;
                            continue;
                        }
                    case state_t::is_2nd_return:
                        if (c == '\n') {
                            state = state_t::is_2nd_newline;
                            reusec = true; // !!
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::is_2nd_newline:
                        //std::cout << "head end" << std::endl;
                        return;
                    case state_t::i_key:
                        if (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '-' || c == '_'){
                            state = state_t::i_key;
                            key += c;
                            continue;
                        }
                        else if (c == ' ') {
                            state = state_t::a_key;
                            continue;
                        }
                        else if (c == ':') {
                            state = state_t::i_colon;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::a_key:
                        if (c == ' ') {
                            continue;
                        }
                        else if (c == ':'){
                            state = state_t::i_colon;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::i_colon:
                        if (c == ' ') {
                            continue;
                        }
                        else if (c > 32 && c < 128){
                            state = state_t::i_val;
                            reusec = true;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    case state_t::i_val:
                        if (c >= 32 && c < 128) {
                            val += c;
                            continue;
                        }
                        else if (c == '\r') {
                            if (key.size()) {
                                _header_lines[key] = val;
                                //std::cout << key << ": \"" << val << "\"" << std::endl;
                            }
                            state = state_t::is_return;
                            continue;
                        }
                        else {
                            goto fail;
                        }
                    default:
                        break;
                    }
                }

            fail:;
            }

        public:
            std::string _verb;
            std::string _uri;
            std::string _uri_decoded;
            std::string _query;
            std::string _version;

            std::vector<unsigned char> _headers;

            std::map<std::string, std::string> _header_lines;
        };

        class http_handler_t {
        public:
            http_handler_t(client_t& client, http_header_t& header)
                : _client(client)
                , _header(header)
            {}

            virtual ~http_handler_t() {
                ::closesocket(_client.fd);
            }

        protected:
            client_t&       _client;
            http_header_t&  _header;

        public:
            virtual void handle() = 0;
        };

        class static_http_handler_t : public http_handler_t {
        public:
            static_http_handler_t(client_t& client, http_header_t& header)
                : http_handler_t(client, header)
                , _keep_alive(false)
            {}

            virtual ~static_http_handler_t() {

            }

        public:
            bool keep_alive() {
                return _keep_alive;
            }
        public:
            virtual void handle() {
                _keep_alive = false;

                using namespace taoweb;

                extern http::error_page_t error_page;

                _restart: // TODO

                auto cd = taoweb::file_system::cur_dir();
                auto file = cd + _header._uri_decoded;

                file_system::file_type file_type;
                file_type = taoweb::file_system::file_attr(file.c_str());
                if (file_type == file_system::file_type::error) {
                    error_page.output(_client.fd, 403);
                    return;
                }
                else if (file_type == file_system::file_type::directory) {
                    if (_header._uri_decoded.back() != '/') {
                        std::stringstream ss;
                        std::string new_uri = _header._uri_decoded + "/";
                        ss << "HTTP/1.1 301 Moved Permanently\r\n"
                            << "Server: taoweb/0.0\r\n"
                            << "Date: " << gmtime() << "\r\n"
                            << "Location: " <<  new_uri << "\r\n"
                            << "\r\n";

                        auto& str = ss.str();

                        ::send(_client.fd, str.c_str(), str.size(), 0);
                        ::closesocket(_client.fd);
                        return;
                    }

                    _header._uri_decoded += "index.html";
                    goto _restart;
                }

                file_system::file_object_t fobj(file.c_str());
                if (!fobj.open()) {
                    error_page.output(_client.fd, 403);
                    return;
                }

                if (_header._header_lines.count("If-None-Match") && _header._header_lines["If-None-Match"] == fobj.etag()) {
                    error_page.output(_client.fd, 304);
                    return;
                }

                std::string err200 = "HTTP/1.1 200 OK\r\n";
                err200 += "Server: taoweb/0.0\r\n";
                err200 += "Date: " + gmtime() + "\r\n";
                err200 += "Content-Length: %d\r\n";
                err200 += "ETag: %s\r\n";
                err200 += "\r\n";

                char buf[10240];
                uint32_t n_read;

                // 被坑了一次, fobj.size() 必须转换成 32 位, 否则输出错误, 格式
                ::sprintf(buf, err200.c_str(), (uint32_t)fobj.size(), fobj.etag().c_str());
                ::send(_client.fd, buf, strlen(buf), 0);

                while (fobj.read((uint8_t*)buf, sizeof(buf), &n_read) && n_read > 0) {
                    ::send(_client.fd, buf, (int)n_read, 0);
                }

                fobj.close();

                _keep_alive = _header._header_lines.count("Connection") && _header._header_lines["Connection"] == "keep-alive";
                if(!_keep_alive) ::closesocket(_client.fd);
                return;
            }
        protected:
            bool _keep_alive;
        };
    }
}
