#include <string>
#include <sstream>
#include <iostream>
#include <regex>
#include <memory>

#include <WinSock2.h>
#include <windows.h>

#include "file_system.h"
#include "http_handler.h"

namespace taoweb {

    bool HTTPHandler::handle_dynamic(HTTPHeader& header) {

        std::smatch matches;
        std::string uri(header.get_uri());

        if(uri == "/about") {
            HTTPHeader response_header;
            response_header.put_status("200", "OK")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                .put("Content-Type", "text/plain")
                ;

            std::ostringstream oss;
            oss << header.get_verb() << " " << header.get_uri() << " " << header.get_ver() << "\r\n";
            header.for_each([&](const std::string& name, const std::string& value) {
                oss << name << ": " << value << "\r\n";
            });

            send(response_header);
            send("Request: ------------------------------------------------------------------\r\n");
            send(oss.str());
            send("\r\n");
            send("Response:------------------------------------------------------------------\r\n");
            send(response_header);
            close();

            return true;
        }
        else if(std::regex_search(uri, matches, std::regex(R"(^/redirect/(.*))"))) {
            HTTPHeader response_header;
            response_header.put_status("302", "Moved Temporarily")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                .put("Location", matches[1])
                ;

            send(response_header);

            close();

            return true;
        }

        return false;
     }

    bool HTTPHandler::handle_command(HTTPHeader& header) {
        std::smatch matches;
        auto uri = header.get_uri();
        if (std::regex_match(uri, matches, std::regex(R"(/cgi-bin/([^/]+\.exe))", std::regex_constants::icase))) {
            HTTPHeader response_header;
            response_header.put_status("200", "OK")
                .put("Server", "taoweb/1.0")
                .put("Date", gmtime())
                .put("Content-Type", "text/plain; charset=gb2312")
                ;

            send(response_header);

            auto cmd = matches[1].str();
            ::system((cmd + " > _tmp").c_str());

            file_system::FileObject file("_tmp");
            file.open();

            file.read_block(1024, [&](const void* buf, int size) {
                send(buf, size);
                return true;
            });

            close();

            return true;
        }

        return false;
    }

    bool HTTPHandler::handle_cgibin(HTTPHeader& header) {
        std::smatch matches;
        auto uri = header.get_uri();

        // 执行子进程。传入命令行，环境变量，输出回调
        using ssmap_t = std::map<std::string, std::string>;
        using callback_t = std::function<void(const char* const data, int cb)>;

        static auto exec = [](const std::string& cmdline, const ssmap_t& env, callback_t output) {

            // 传递给子进程用的 标准读读句柄，标准写写句柄，标准出错写句柄
            HANDLE hStdInRead = nullptr, hStdInWrite = nullptr, hStdInWriteTemp = nullptr;
            HANDLE hStdOutRead = nullptr, hStdOutWrite = nullptr, hStdOutReadTemp = nullptr;
            HANDLE hStdErrWrite = nullptr;

            // 当前进程句柄与句柄继承选项
            HANDLE hProcess;
            DWORD dwDupOpt;

            // 被创建的用于提供给的子进程的管道应该被继承，不然没法用
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.lpSecurityDescriptor = nullptr;
            sa.bInheritHandle = TRUE;

            try {
                // 创建通信管道
                if (!::CreatePipe(&hStdOutReadTemp, &hStdOutWrite, &sa, 0))
                    throw "CreatePipe() fail";

                if (!::CreatePipe(&hStdInRead, &hStdInWriteTemp, &sa, 0))
                    throw "CreatePipe() fail";

                hProcess = ::GetCurrentProcess();
                dwDupOpt = DUPLICATE_SAME_ACCESS;


                // 复制标准写为标准出错，使它们共用
                if (!::DuplicateHandle(hProcess, hStdOutWrite, hProcess, &hStdErrWrite, 0, TRUE, dwDupOpt))
                    throw "DuplicateHandle() fail";

                // 使当前进程的标准写读句柄，标准读写句柄不可继承，不然的话
                // 子进程继承了句柄退出的时候会导致 ReadFile 被挂起。

                if (!::DuplicateHandle(hProcess, hStdOutReadTemp, hProcess, &hStdOutRead, 0, FALSE, dwDupOpt))
                    throw "DuplicateHandle() fail";

                if (!::DuplicateHandle(hProcess, hStdInWriteTemp, hProcess, &hStdInWrite, 0, FALSE, dwDupOpt))
                    throw "DuplicateHandle() fail";
            }
            catch (...) {
                if (hStdOutRead)     ::CloseHandle(hStdOutRead);
                if (hStdOutWrite)    ::CloseHandle(hStdOutWrite);
                if (hStdOutReadTemp) ::CloseHandle(hStdOutReadTemp);
                if (hStdInRead)      ::CloseHandle(hStdInRead);
                if (hStdInWrite)     ::CloseHandle(hStdInWrite);
                if (hStdInWriteTemp) ::CloseHandle(hStdInWriteTemp);
                if (hStdErrWrite)    ::CloseHandle(hStdErrWrite);

                return;
            }

            // 准备环境变量
            std::string envstr([&] {
                // 当前的
                char* cur_env = GetEnvironmentStrings();
                if (!cur_env) {
                    cur_env = "";
                }

                // 遍历到最后准备追加
                char* p = cur_env;
                while (*p) {
                    while (*p++)
                        ;
                }

                // 附加环境变量（CGI变量）
                std::string cur_str(cur_env, p);
                for (const auto& it : env) {
                    cur_str += it.first + '=' + it.second + '\0';
                }

                return std::move(cur_str);
            }());

            // 创建子进程（带重定向和环境变量）
            PROCESS_INFORMATION pi;
            STARTUPINFO si = { 0 };
            si.cb = sizeof(STARTUPINFO);
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.hStdInput = hStdInRead;
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdErrWrite;
            si.wShowWindow = SW_HIDE;

            std::unique_ptr<char[]> cmdline_modifiable(new char[cmdline.size() + 1]);
            ::memcpy(cmdline_modifiable.get(), cmdline.c_str(), cmdline.size() + 1);

            bool bOK = !!::CreateProcess(nullptr, cmdline_modifiable.get(),
                nullptr, nullptr, TRUE, // 子进程需要继承句柄
                CREATE_NEW_CONSOLE, (void*)envstr.c_str(), nullptr, &si, &pi);

            // 关闭不再需要的句柄
            ::CloseHandle(hStdInRead);
            ::CloseHandle(hStdOutWrite);
            ::CloseHandle(hStdErrWrite);

            if (!bOK) {
                ::CloseHandle(hStdInWrite);
                ::CloseHandle(hStdOutRead);
                return;
            }

            // 准备读管道数据
            const int buf_size = 4096;
            char buf[buf_size];
            DWORD dwRead;

            // 循环读
            while (::ReadFile(hStdOutRead, (void*)&buf[0], buf_size, &dwRead, nullptr) && dwRead > 0) {
                output(buf, dwRead);
            }

            // 读结束
            DWORD dwErr = ::GetLastError();
            if (dwErr == ERROR_BROKEN_PIPE || dwErr == ERROR_NO_DATA) {

            }

            ::CloseHandle(hStdInWrite);
            ::CloseHandle(hStdOutRead);
        };

        if(std::regex_match(uri, matches, std::regex(R"(/cgi-bin/([^/]+\.lua))", std::regex_constants::icase))) {
            auto command = R"(D:\YangTao\tools\lua-5.3\lua53.exe)";
            auto script = file_system::exe_dir() + '/' + matches[1].str();

            auto command_line = std::string(R"(")") + command + R"(" ")" + script + '"';

            ssmap_t env;
            env["TEST"] = "TESTTESTTEST";

            exec(command_line, env, [&](const char* buf, int size) {
                send(buf, size);
            });

            close();

            return true;
        }
        else if(std::regex_match(uri, matches, std::regex(R"(/cgi-bin/([^/]+\.bat))", std::regex_constants::icase))) {
            auto script = file_system::exe_dir() + '/' + matches[1].str();
            auto command_line = '"' + script + '"';

            ssmap_t env;

            exec(command_line, env, [&](const char* buf, int size) {
                send(buf, size);
            });
            
            close();

            return true;
        }
        else if (std::regex_match(uri, matches, std::regex(R"(/cgi-bin/(.+\.php))", std::regex_constants::icase))) {
            auto command = R"(D:\YangTao\utilities\php7\php-cgi.exe)";
            auto script = file_system::exe_dir() + '/' + matches[1].str();

            auto command_line = std::string(R"(")") + command + R"(" ")" + script + '"';

            ssmap_t env;
            env["TEST"] = "TESTTESTTEST";
            env["HTTP_USER_AGENT"] = "taoweb/1.0";

            send("HTTP/1.1 200 OK\r\n");

            exec(command_line, env, [&](const char* buf, int size) {
                send(buf, size);
            });
            
            close();

            return true;
        }

        return false;
    }

    bool HTTPHandler::handle_static(HTTPHeader& header) {
        using string = std::string;
        using namespace file_system;

        _l_rewrite:
        
        string path = exe_dir() + '/' + header.get_uri();
        FileType ty = type(path.c_str());

        if(ty == FileType::directory) {
            if(header.get_uri() != "/" && path.back() != '/') {
                HTTPHeader response_header;
                response_header.put_status("301", "Moved Permanently")
                    .put("Server", "taoweb/1.0")
                    .put("Date", gmtime())
                    .put("Location", header.get_uri() + '/')
                    ;

                send(response_header);
                close();
            }
            else {
                auto newuri = exe_dir() + '/' + header.get_uri() + "index.html";
                if (file_system::exists(newuri.c_str())) {
                    header.set_uri(header.get_uri() + "index.html");
                    goto _l_rewrite;
                }
                else {
                    HTTPHeader response_header;
                    response_header.put_status("403", "forbidden")
                        .put("Server", "taoweb/1.0")
                        .put("Date", gmtime())
                        ;

                    send(response_header);

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
                    send(body);

                    close();
                }
            }
        }
        else if(ty == FileType::not_found) {
            HTTPHeader header;
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
        else if(ty == FileType::access_denied || ty == FileType::error) {
            HTTPHeader header;
            header.put_status("403", "Forbidden")
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
        else if(ty == FileType::file) {
            FileObject file(path.c_str());

            file.open();

            Stat* st = file.stat();

            if(header.get("If-None-Match") == file.etag()) {
                HTTPHeader header;
                header.put_status("304", "Not Modified")
                    .put("Server", "taoweb/1.0")
                    .put("Date", gmtime())
                    ;

                send(header);
                close();
            }
            else {

                HTTPHeader header;
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
                    return true;
                });

                close();
            }
        }

        return true;
    }

    void HTTPHandler::send(const void* buf, int cb) {
        int sent = 0;
        static std::string s;
        while(sent < cb) {
            int r = ::send(_client.fd, (const char*)buf + sent, cb - sent, 0);
            if(r == 0)
                throw "客户端主动关闭了连接。";
            else if(r == -1) {
                
                s = "发送数据时遇到错误。" + std::to_string(_client.fd) + ": " + std::to_string(::GetLastError());
                throw s.c_str();
            }
            else
                sent += r;
        }
    }

    void HTTPHandler::send(const std::string& s) {
        send(s.c_str(), s.size());
    }

    void HTTPHandler::recv(void* buf, int cb) {
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

    void HTTPHandler::close() {
        ::closesocket(_client.fd);
    }

    void HTTPHandler::handle() {
        HTTPHeader header;
        header.read(_client.fd);

        // 连接被提前关闭
        // 或不合法的请求头部
        if (header.empty()) {
            close();
            return;
        }

        0
        || handle_dynamic(header)
        || handle_cgibin(header)
        || handle_command(header)
        || handle_static(header)
        ;
    }

}
