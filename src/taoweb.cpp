#include <string>
#include <thread>
#include <iostream>

#include "http_base.h"
#include "http_handler.h"

using namespace taoweb;

int main()
{
    init_winsock();

    try {
        SocketServer server("127.0.0.1", 80);
        server.start();

        Client c;
        while(server.accept(&c)) {

            std::cout << "accept: " << c.fd << std::endl;

            auto proc = [](Client c) {
                try {
                    HTTPHandler handler(c);
                    handler.handle();
                }
                catch (const char* err) {
                    std::cerr << err << std::endl;
                }
            };

            std::thread(proc, c).detach();
        }
    }
    catch (const char* err) {
        std::cerr << err << std::endl;
    }

	return 0;
}
