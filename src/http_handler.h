#pragma once

#include "http_base.h"

namespace taoweb {
    class http_handler_t
    {
    public:
        http_handler_t(client_t client)
            : _client(client) {}

        void handle();

    protected:
        bool handle_static(http_header_t& header);
        bool handle_dynamic(http_header_t& header);
        bool handle_command(http_header_t& header);
        bool handle_cgi_bin(http_header_t& header);

    protected:
        void close();
        void send(const std::string& s);
        void send(const void* buf, int cb);
        void recv(void* buf, int cb);

    protected:
        client_t        _client;
    };
}
