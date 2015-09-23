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


static int                                  threads_created;
static taoweb::lock_count                   threads;
static taoweb::lock_queue<taoweb::client_t> clients;

unsigned int __stdcall handler_thread(void* ud) {
    taoweb::client_t client;
    while ((true)) {
        threads.inc();
        client = clients.pop();
        threads.dec();

        // handler here
        taoweb::http::http_header_t header;
        header.read_headers(client.fd);
        if (!header._header_lines.size())
            continue;

        taoweb::http::static_http_handler_t handler(client, header);
        handler.handle();
        if (handler.keep_alive()) {
            clients.push(client);
        }
    }

    return 0;
}

void create_worker_thread(taoweb::client_t& client) {
    clients.push(client);
    if (threads.size() < 3) {
        HANDLE thr = (HANDLE)_beginthreadex(NULL, 0, handler_thread, NULL, 0, NULL);
        CloseHandle(thr);
        std::cout << "threads created: " << threads_created << ", threads spare: " << threads.size() << std::endl;
    }
}

int main()
{
    using namespace taoweb;

    win_sock _wsa;

    socket_server server("127.0.0.1", 8080, 64);

    server.start();

    client_t client;
    while (server.accept(&client)) {
        create_worker_thread(client);
    }

	return 0;
}
