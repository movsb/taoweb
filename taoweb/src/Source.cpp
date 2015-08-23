#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <cassert>
#include <iostream>
#include <locale>
#include <cstdio>
#include <queue>
#include <process.h>

#include <WinSock2.h>

#include "threading.hpp"

#include "socket.hpp"
#include "http_base.hpp"
#include "http_handler.hpp"

taoweb::http::error_page_t error_page;



unsigned int __stdcall worker_thread(void* ud) {
    auto& queue = *reinterpret_cast<taoweb::client_queue*>(ud);
    taoweb::client_t* c = nullptr;

    while (c = &queue.pop()) {
        taoweb::http::http_header_t header;
        header.read_headers(c->fd);

        if (!header._header_lines.size())
            continue;

        taoweb::http::static_http_handler_t handler(*c, header);
        handler.handle();
        if (handler.keep_alive()) {
            queue.push(*c);
        }
    }

    return 0;
}

void create_worker_threads(taoweb::client_queue& queue) {
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);

    DWORD n_threads = si.dwNumberOfProcessors * 2;
    for (int i = 0; i < (int)n_threads; i++){
        _beginthreadex(nullptr, 0, worker_thread, &queue, 0, nullptr);
    }
}

int main()
{
    using namespace taoweb;

    client_queue queue;
    create_worker_threads(queue);

    win_sock _wsa;

    socket_server server("127.0.0.1", 80, 128);

    server.start();

    client_t client;
    while (server.accept(&client)) {
        queue.push(client);
        std::cout << "push\n";
    }

	return 0;
}
