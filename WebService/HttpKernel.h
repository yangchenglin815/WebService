#pragma once
#include "MemBuffer.h"
#include <uv.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <memory.h>
#include <ctype.h>

#define WEB_SERVICE_VERSION "0.0.1"

typedef char* (*web_res)(const char* request_header, const char* path_info, const char* payload);

int service_init(const char* static_path, const char* ip, int port);
char* format_http_respond(const char* status, const char* content_type, const char* cookie,
	const void* content, int content_length, int* response_size);
char* web_errorpage(int error_code, const char* error_info);
char* web_response(const int code, const char* content_type, const char* cookie, const char* content);

int web_handle_request(web_res response_get, web_res response_post);
int web_par(char* router_info, char* path_info);
char* web_header_parser(char* http_header, char* key);
char* web_path_parser(char* path, int i);

