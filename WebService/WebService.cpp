// WebService.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "HttpKernel.h"
#include "MemBuffer.h"

char* request_get(const char* head, const char * path, const char* query)
{
	char* context = "text/html";
	char* cookie = "";

	return web_response(200, context, cookie, query);
}

char* request_post(const char* head, const char * path, const char* payload)
{
	char* context = "text/html";
	char* cookie = "";

	return web_response(200, context, cookie, payload);
}

int main()
{
	char* static_path = "E:\\manage-system\\manage-system\\dist";
	char* ip = "0.0.0.0";

	web_handle_request(request_get, request_post);
	service_init(static_path, ip, 8081);
}

