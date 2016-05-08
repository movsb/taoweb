#include <string>
#include <sstream>
#include <iostream>

#include <WinSock2.h>
#include <windows.h>

#include "file_system.h"
#include "http_handler.h"

namespace taoweb {

    bool http_handler_t::handle_dynamic(http_header& header) {
        if(header.get_uri() == "/about") {
            http_header response_header;
            response_header.put_status("200", "OK")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                .put("Content-Type", "text/plain")
                ;

            std::ostringstream oss;
            oss << header.get_verb() << " " << header.get_uri() << " " << header.get_ver() << "\r\n";
            for(auto& it : header.get_headers()) {
                oss << it.first << ": " << it.second << "\r\n";
            }

            send(response_header);
            send("Request:\r\n");
            send(oss.str());
            send("\r\n");
            send("Response:\r\n");
            send(response_header);
            close();

            return true;
        }

        return false;
    }

    void http_handler_t::handle() {
        using string = std::string;
        using namespace file_system;

        http_header header;
        header.read(_client.fd);

        // prematurely closed connection
        if(header.get_headers().empty())
            return;

        if(handle_dynamic(header))
            return;

        _l_rewrite:
        
        string path = exe_dir() + '/' + header.get_uri();
        file_type ty = type(path.c_str());

        if(ty == file_type::directory) {
            if(header.get_uri() != "/" && path.back() != '/') {
                http_header response_header;
                response_header.put_status("301", "Moved Permanently")
                    .put("Server", "taoweb/1.0")
                    .put("Date", gmtime())
                    .put("Location", header.get_uri() + '/')
                    ;
                send(response_header);
                close();
            }
            else {
                header.set_uri(header.get_uri() + "index.html");
                goto _l_rewrite;
            }
        }
        else if(ty == file_type::not_found) {
            http_header header;
            header.put_status("404", "Not Found")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                ;

            
            string body(
R"(<!doctype html>
<html>
<head>
    <title>404 - Not Found</title>
</head>
<body>
    <center>
        <h1>404 - Not Found</h1>
        <hr />
        <p>taoweb/1.0</p>
    </center>
</body>
</html>
)");

            send(header);
            send(body);
            close();
        }
        else if(ty == file_type::access_denied || ty == file_type::error) {
            http_header header;
            header.put_status("404", "Not Found")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                ;


            string body(
R"(<!doctype html>
<html>
<head>
    <title>403 - Forbidden</title>
</head>
<body>
    <center>
        <h1>403 - Forbidden</h1>
        <hr />
        <p>taoweb/1.0</p>
    </center>
</body>
</html>
)");

            send(header);
            send(body);
            close();
        }
        else if(ty == file_type::file) {
            file_object_t file(path.c_str());

            file.open();

            stat_t* st = file.stat();

            if(header.get("If-None-Match") == file.etag()) {
                http_header header;
                header.put_status("304", "Not Modified")
                    .put("Server", "taoweb/1.0")
                    .put("Date", gmtime())
                    ;

                send(header);
                close();
            }
            else {

                http_header header;
                header.put_status("200", "OK")
                    .put("Server", "taoweb/1.0")
                    .put("Date", gmtime())
                    .put("Content-Type", mime(path))
                    .put("Content-Length", std::to_string(file.size()))
                    .put("ETag", file.etag())
                    ;

                send(header);

                file.read_block(4096, [&](const void* buf, int size) {
                    send(buf, size);
                });

                close();
            }
        }
    }

    void http_handler_t::send(const void* buf, int cb) {
        int sent = 0;
        static std::string s;
        while(sent < cb) {
            int r = ::send(_client.fd, (const char*)buf + sent, cb - sent, 0);
            if(r == 0)
                throw "客户端主动关闭了连接。";
            else if(r == -1) {
                
                s = "发送数据时遇到错误。" + std::to_string(_client.fd);
                throw s.c_str();
            }
            else
                sent += r;
        }
    }

    void http_handler_t::send(const std::string& s) {
        send(s.c_str(), s.size());
    }

    void http_handler_t::recv(void* buf, int cb) {
        int recved = 0;

        while(recved < cb) {
            int r = ::recv(_client.fd, (char*)buf, cb - recved, 0);
            if(r == 0)
                throw "客户端主动关闭了连接。";
            else if(r == -1)
                throw "接收数据时遇到错误。";
            else
                recved += r;
        }
    }

    void http_handler_t::close() {
        ::closesocket(_client.fd);
    }
}
