#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <map>
#include <locale>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

/*
  A-Z a-z 0-9
  - . _ ~ : / ? # [ ] @ ! $ & ' ( ) * + , ; =
  no space
*/
bool decode_uri(const std::string& uri, std::string* result){
	if(uri.size() == 0 || !result) return false;

	auto p = uri.c_str();
	auto q = p + uri.size();

	for(; p < q;){
		auto c = *p;
		if(c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || strchr("-._~:/?#[]@!$&'()*+,;=", c)){
			*result += c;
			p++;
		}
		else if(c == '%'){
			if(p + 1 < q && p[1] == '%'){
				*result += '%';
				p++;
			}

			if(p + 2 < q){
				unsigned char v = 0;
				p++;
				if(*p >= '0' && *p <= '9') v = *p - '0';
				else if(*p >= 'A' && *p <= 'F') v = *p - 'A' + 10;
				else return false;
				p++;
				v = v * 16;
				if(*p >= '0' && *p <= '9') v += *p - '0';
				else if(*p >= 'A' && *p <= 'F') v += *p - 'A' + 10;
				else return false;

				p++;
				*result += v;
			}
			else {
				return false;
			}
		}
		else {
			return false;
		}
	}
	return true;
}

class http_header_t {
public:
	void read_headers(SOCKET& fd) {
		unsigned char c;
		int r;

		enum class state_t {
			is_return, is_newline, is_2nd_return, is_2nd_newline,
			b_verb, i_verb, a_verb,
			i_uri, a_uri,
			i_version, a_version,
			i_key, a_key, i_val, a_val, i_colon,
		};

		state_t state = state_t::b_verb;
		bool reusec = false;
		std::string key, val;

		while(reusec || (r = ::recv(fd, (char*)&c, 1, 0)) == 1){
			//if(!reusec) std::cout << c;
			reusec = false;
			switch(state)
			{
			case state_t::b_verb:
			case state_t::i_verb:
				if(c >= 'A' && c <= 'Z'){
					state = state_t::i_verb;
					_verb += c;
					continue;
				}
				else {
					if(_verb.size()) {
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
				if(c == ' ' || c == '\t') {
					continue;
				}
				else if(c > 32 && c < 128) {
					state = state_t::i_uri;
					reusec = true;
					continue;
				}
				else {
					goto fail;
				}
				break;
			case state_t::i_uri:
				if(c > 32 && c < 128) {
					_uri += c;
					continue;
				}
				else {
					::decode_uri(_uri, &_uri_decoded);
					state = state_t::a_uri;
					reusec = true;
					continue;
				}
			case state_t::a_uri:
				if(c == ' ' || c == '\t') {
					continue;
				}
				else {
					state = state_t::i_version;
					reusec = true;
					continue;
				}
			case state_t::i_version:
				if(c > 32 && c < 128) {
					_version += c;
					continue;
				}
				else {
					std::cout << _verb << " " << _uri << " " << _version << std::endl;
					state = state_t::a_version;
					reusec = true;
					continue;
				}
			case state_t::a_version:
				if(c == ' ' || c == '\t'){
					continue;
				}
				else if(c == '\r') {
					state = state_t::is_return;
					continue;
				}
				else {
					goto fail;
				}
			case state_t::is_return:
				if(c == '\n') {
					state = state_t::is_newline;
					continue;
				}
				else {
					goto fail;
				}
			case state_t::is_newline:
				key = "", val = "";
				if(c == '\r') {
					state = state_t::is_2nd_return;
					continue;
				}
				else {
					state = state_t::i_key;
					reusec = true;
					continue;
				}
			case state_t::is_2nd_return:
				if(c == '\n') {
					state = state_t::is_2nd_newline;
					reusec = true; //¡¡!!
					continue;
				}
				else {
					goto fail;
				}
			case state_t::is_2nd_newline:
				std::cout << "head end" << std::endl;
				return;
			case state_t::i_key:
				if(c>='a' && c<='z' || c>='A' && c<='Z' || c=='-' || c=='_'){
					state = state_t::i_key;
					key += c;
					continue;
				}
				else if(c == ' ') {
					state = state_t::a_key;
					continue;
				}
				else if(c == ':') {
					state = state_t::i_colon;
					continue;
				}
				else {
					goto fail;
				}
			case state_t::a_key:
				if(c == ' ') {
					continue;
				}
				else if(c == ':'){
					state = state_t::i_colon;
					continue;
				}
				else {
					goto fail;
				}
			case state_t::i_colon:
				if(c == ' ') {
					continue;
				}
				else if(c > 32 && c < 128){
					state = state_t::i_val;
					reusec = true;
					continue;
				}
				else {
					goto fail;
				}
			case state_t::i_val:
				if(c >= 32 && c < 128) {
					val += c;
					continue;
				}
				else if(c == '\r') {
					if(key.size()) {
						_header_lines[key] = val;
						std::cout << key << ": \"" << val << "\"" << std::endl;
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
	std::string _version;

	std::vector<unsigned char> _headers;

	std::map<std::string, std::string> _header_lines;
};

std::string cur_dir() {
	char path[MAX_PATH];
	::GetModuleFileName(nullptr, path, sizeof(path));
	*strrchr(path, '\\') = '\0';

	return path;
}

void error_response(SOCKET& fd, int code) {
	const char* err404 =
		"HTTP/1.1 404 Not Found\r\n"
		"Server: taoweb\r\n"
		"\r\n"
		"<html>\n"
		"<head><title>404 Not Found</title></head>\n"
		"<body><center><h1>404 Not Found</h1><hr>taoweb</center></body>\n"
		"</html>\n";

	if(code == 400){
		send(fd, err404, strlen(err404), 0);
		closesocket(fd);
	}
	else {
		closesocket(fd);
	}
}

bool respond(SOCKET& fd, http_header_t& header){
	auto cd = cur_dir();
	auto file = cd + header._uri_decoded;

	HANDLE h_file = CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(h_file == INVALID_HANDLE_VALUE){
		error_response(fd, 400);
		return false;
	}

	BY_HANDLE_FILE_INFORMATION file_info = { 0 };
	GetFileInformationByHandle(h_file, &file_info);

	DWORD file_size = file_info.nFileSizeLow;
	char etag[19];
	sprintf(etag, "\"%08X%08X\"", file_info.nFileIndexHigh, file_info.nFileIndexLow);

	if (header._header_lines.count("If-None-Match") && header._header_lines["If-None-Match"] == etag) {
		CloseHandle(h_file);

		const char* err304 =
			"HTTP/1.1 304 Not Modified\r\n"
			"Server: taoweb\r\n"
			"\r\n";

		send(fd, err304, strlen(err304), 0);
		closesocket(fd);
		return true;
	}

	const char* err200 =
		"HTTP/1.1 200 OK\r\n"
		"Server: taoweb\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: %d\r\n"
		"ETag: %s\r\n"
		"\r\n";

	char buf[10240];
	DWORD n_read;
	sprintf(buf, err200, file_size, etag);
	send(fd, buf, strlen(buf), 0);

	while (ReadFile(h_file, buf, sizeof(buf), &n_read, nullptr) && n_read > 0){
		send(fd, buf, (int)n_read, 0);
	}

	CloseHandle(h_file);
	closesocket(fd);

	return true;
}

int main()
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET sfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in server_addr = {0};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(80);

	if(bind(sfd, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
		throw "err bind";

	if(listen(sfd, 8) == SOCKET_ERROR)
		throw "err listen";

	SOCKET cfd;
	sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);
	while((cfd = accept(sfd, (sockaddr*)&client_addr, &client_addr_len)) != INVALID_SOCKET){
		http_header_t header;
		header.read_headers(cfd);
		respond(cfd, header);
	}

	return 0;
}