#pragma once

#include "logger.hpp"
#include "http_base.h"

namespace taoweb {
    class HTTPHandler
    {
    public:
        HTTPHandler(Client client)
            : _client(client) {}

        ~HTTPHandler() {
            close();
        }

        void handle();

    protected:
        bool handle_static(HTTPHeader& header);
        bool handle_dynamic(HTTPHeader& header);
        bool handle_command(HTTPHeader& header);
        bool handle_cgibin(HTTPHeader& header);

    protected:
        void close();
        void send(const std::string& s);
        void send(const void* buf, int cb);
        void recv(void* buf, int cb);

    protected:
        Client        _client;
    };
}
