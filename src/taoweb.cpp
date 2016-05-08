#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>

#include "http_base.h"
#include "http_handler.h"

using namespace taoweb;



int main()
{
    init_winsock();

    try {
        socket_server_t server("127.0.0.1", 80);
        server.start();

        std::atomic<bool> running;
        client_t c;
        while(server.accept(&c)) {
            running = false;
            std::thread([&]() {
                try {
                    http_handler_t handler(c);
                    running = true;
                    handler.handle();
                }
                catch(const char* err) {
                    std::cerr << err << std::endl;
                }
            }).detach();

            while(!running)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (const char* err) {
        std::cerr << err << std::endl;
    }

	return 0;
}
