#include <string>
#include <thread>
#include <iostream>

#include "logger.hpp"
#include "http_base.h"
#include "http_handler.h"

using namespace taoweb;

taoweb::Logger g_logger;

int main()
{
    init_winsock();

    g_logger.info("Program started...\n");

    try {
        SocketServer server("127.0.0.1", 80);
        server.start();

        Client c;

        g_logger.info("Wait for connection...\n");

        while(server.accept(&c)) {
            auto proc = [](Client c) {
                try {
                    HTTPHandler handler(c);
                    handler.handle();
                }
                catch (const char* err) {
                    g_logger.err("%s\n", err);
                }
            };

            g_logger.info("Accepted client %s:%d, fd=%d\n", inet_ntoa(c.addr), c.port, c.fd);

            std::thread(proc, c).detach();
        }
    }
    catch (const char* err) {
        std::cerr << err << std::endl;
    }

	return 0;
}
