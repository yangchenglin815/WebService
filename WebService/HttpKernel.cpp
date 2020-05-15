#include "stdafx.h"
#include "HttpKernel.h"

uv_tcp_t _server;
uv_tcp_t _client;

const char* STATIC_PATH = NULL;

static void on_connection_cb(uv_stream_t* server, int status);

int service_init(const char* static_path, const char* ip, int port)
{
	struct sockaddr_in addr;
	uv_ip4_addr(ip && ip[0] ? ip : "0.0.0.0", port, &addr);
	STATIC_PATH = static_path;

	uv_tcp_init(uv_default_loop(), &_server);
	uv_tcp_bind(&_server, (const struct sockaddr*)&addr, 0);
	uv_listen((uv_stream_t*)&_server, 8, on_connection_cb);

	return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

static void after_uv_close_client(uv_handle_t* handle)
{
	membuf_uninit((membuf_t*)handle->data);
	free(handle->data);
	free(handle);
}

static void after_uv_write(uv_write_t* w, int status)
{
	uv_close((uv_handle_t*)w->handle, after_uv_close_client);
	free(w);
}

static void close_client(uv_stream_t* client)
{
	uv_close((uv_handle_t*)client, after_uv_close_client);
}

static void write_uv_data(uv_stream_t* client, const void* data, unsigned int len,
	int need_copy_data, int need_free_data)
{
	uv_buf_t buf;
	uv_write_t* w;
	void* newdata = (void*)data;

	if (data == NULL || len == 0)
		return;

	if (len == (unsigned int)-1)
		len = strlen((char*)data);

	if (need_copy_data) {
		newdata = malloc(len);
		memcpy(newdata, data, len);
	}

	buf = uv_buf_init((char*)newdata, len);
	w = (uv_write_t*)malloc(sizeof(uv_write_t));
	w->data = (need_copy_data || need_free_data) ? newdata : NULL;
	uv_write(w, client, &buf, 1, after_uv_write);
}

char* format_http_respond(const char* status, const char* content_type, const char* cookie,
	const void* content, int content_length, int* response_size)
{
	int totalsize, header_size;
	char* response;

	if (content_length < 0)
		content_length = content ? strlen((char*)content) : 0;

	totalsize = strlen(status) + strlen(content_type) + content_length + 128;
	response = (char*)malloc(totalsize);

	if (cookie)
	{
		header_size = sprintf(response, "HTTP/1.1 %s\r\n"
			"Server: version/%s\r\n"
			"Content-Type: %s; charset=utf-8\r\n"
			"Content-Length: %d\r\n"
			/*"Access-Control-Allow-Origin: *\r\n"*/
			"Set-Cookie: %s\r\n\r\n", 
			status, WEB_SERVICE_VERSION, content_type, content_length, cookie);
	}
	else
	{
		header_size = sprintf(response, "HTTP/1.1 %s\r\n"
			"Server: version/%s\r\n"
			/*"Access-Control-Allow-Origin: *\r\n"*/
			"Content-Type: %s; charset=utf-8\r\n"
			"Content-Length: %d\r\n\r\n",
			status, WEB_SERVICE_VERSION, content_type, content_length);
	}
	assert(header_size > 0);

	if (content)
		memcpy(response + header_size, content, content_length);

	if (response_size)
		*response_size = header_size + content_length;

	return response;
}

static char* handle_status_code(int code)
{
	switch (code)
	{
	case 200: return("200 OK"); break;
	case 301: return("301 Moved Permanently"); break;
	case 302: return("302 Found"); break;
	case 403: return("403 Forbidden"); break;
	case 404: return("404 Not Found"); break;
	case 500: return("500 Internal Server Error"); break;
	case 502: return("502 Bad Gateway"); break;
	case 504: return("504 Gateway Timeout"); break;
	case 100: return("100 Continue"); break;
	case 101: return("101 Switching Protocals"); break;
	case 201: return("201 Created"); break;
	case 202: return("202 Accepted"); break;
	case 203: return("203 Non-authoritative Information"); break;
	case 204: return("204 No Content"); break;
	case 205: return("205 Reset Content"); break;
	case 206: return("206 Partial Content"); break;
	case 300: return("300 Multiple Choices"); break;
	case 303: return("303 See Other"); break;
	case 304: return("304 Not Modified"); break;
	case 305: return("305 Use Proxy"); break;
	case 306: return("306 Unused"); break;
	case 307: return("307 Temporary Redirect"); break;
	case 400: return("400 Bad Request"); break;
	case 401: return("401 Unauthorized"); break;
	case 402: return("402 Payment Required"); break;
	case 405: return("405 Method Not Allowed"); break;
	case 406: return("406 Not Acceptable"); break;
	case 407: return("407 Proxy Authentication Required"); break;
	case 408: return("408 Request Timeout"); break;
	case 409: return("409 Conflict"); break;
	case 410: return("410 Gone"); break;
	case 411: return("411 Length Required"); break;
	case 412: return("412 Precondition Failed"); break;
	case 413: return("413 Request Entity Too Large"); break;
	case 414: return("414 Request-url Too Long"); break;
	case 415: return("415 Unsupported Media Type"); break;
	case 416: return("416 Requested Range Not Satisfiable"); break;
	case 417: return("417 Expectation Failed"); break;
	case 501: return("501 Not Implemented"); break;
	case 503: return("503 Service Unavailable"); break;
	case 505: return("505 HTTP Version Not Supported"); break;
	default: return("200 OK"); break;
	}
}

static char* handle_content_type(const char* type)
{
	if (strcmp(type, "html") == 0 || strcmp(type, "htm") == 0)
		return "text/html";
	else if (strcmp(type, "js") == 0)
		return "text/javascript";
	else if (strcmp(type, "css") == 0)
		return "text/css";
	else if (strcmp(type, "jpeg") == 0 || strcmp(type, "jpg") == 0)
		return "image/jpeg";
	else if (strcmp(type, "png") == 0)
		return "image/png";
	else if (strcmp(type, "gif") == 0)
		return "image/gif";
	else if (strcmp(type, "txt") == 0)
		return "text/plain";
	else
		return "application/octet-stream";
}

#if defined(WIN32)
   #define snprintf _snprintf
#endif

char* web_errorpage(int error_code, const char* error_info)
{
	const char* error = handle_status_code(error_code);
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "<html><head><title>%s</title></head><body bgcolor='white'><center><h1>%s</h1></center><hr><center>Igropyr/%s</center><p>%s</p></body></html>", error, error, WEB_SERVICE_VERSION, error_info);
	return format_http_respond(error, "text/html", NULL, buffer, -1, NULL);
}

char* web_response(const int code, const char* content_type, const char* cookie, const char* content)
{
	char* status = handle_status_code(code);
	return format_http_respond(status, content_type, cookie, content, -1, NULL);
}

static void send_file(uv_stream_t* client, const char* content_type, const char* file, const char* file_path)
{
	int file_size;
	int read_bytes;
	int response_size;
	char* response;
	unsigned char* file_data;

	FILE* fp = fopen(file, "rb");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		file_size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		file_data = (unsigned char*)malloc(file_size);
		read_bytes = fread(file_data, 1, file_size, fp);
		assert(read_bytes == file_size);
		fclose(fp);

		response_size = 0;
		response = format_http_respond("200 OK", content_type, NULL, file_data, file_size, &response_size);
		free(file_data);
		write_uv_data(client, response, response_size, 0, 1);
	}
	else
	{
		response = web_errorpage(404, file_path);
		write_uv_data(client, response, -1, 0, 1);
	}
}

web_res res_get;
web_res res_post;

int web_handle_request(web_res response_get, web_res response_post)
{
	res_get = response_get;
	response_post = response_post;

	return 1;
}

static void handle_get(uv_stream_t* client, const char* request_header, const char* path_info, const char* query_string)
{
	const char* postfix = strrchr(path_info, '.');
	if (postfix)
	{
		postfix++;
		if (STATIC_PATH)
		{
			char file[1024];
			snprintf(file, sizeof(file), "%s%s", STATIC_PATH, path_info);
			send_file(client, handle_content_type(postfix), file, path_info);
			return;
		}
		else
		{
			char* response = web_errorpage(403, path_info);
			write_uv_data(client, response, -1, 0, 1);
			return;
		}
	}
	else
	{
		char* respone = res_get(request_header, path_info, query_string);
		if (*respone == ' ')
		{
			if (STATIC_PATH)
			{
				char* file_path = respone;
				file_path++;
				postfix = strrchr(file_path, '.');
				postfix++;
				char file[1024];
				snprintf(file, sizeof(file), "%s%s", STATIC_PATH, file_path);
				send_file(client, handle_content_type(postfix), file, path_info);
				return;
			}
			else
			{
				char* respone = web_errorpage(403, path_info);
				write_uv_data(client, respone, -1, 0, 1);
				return;
			}
		}
		else
		{
			write_uv_data(client, respone, -1, 0, 1);
			return;
		}
	}
}

static void handle_post(uv_stream_t* client, const char* request_header, const char* path_info, const char* payload)
{
	char* respone = res_post(request_header, path_info, payload);

	if (*respone == ' ')
	{
		if (STATIC_PATH)
		{
			char* file_path = respone;
			file_path++;
			char* postfix = strrchr(file_path, '.');
			postfix++;
			char file[1024];
			snprintf(file, sizeof(file), "%s%s", STATIC_PATH, file_path);
			send_file(client, handle_content_type(postfix), file, path_info);
			return;
		}
		else
		{
			char* respone = web_errorpage(403, path_info);
			write_uv_data(client, respone, -1, 0, 1);
			return;
		}
	}
	else
	{
		write_uv_data(client, respone, -1, 0, 1);
		return;
	}
}

static void on_uv_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf)
{
	*buf = uv_buf_init((char*)malloc(suggested_size), suggested_size);
}

static void on_uv_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf)
{
	if (nread > 0)
	{
		char* separator;
		char* payload;
		char* query_stirng;
		char* end;

		membuf_t* membuf = (membuf_t*)client->data;
		assert(membuf);
		membuf_append_data(membuf, buf->base, nread);
		separator = (char*)strstr((const char*)membuf->data, "\r\n\r\n");
	
		if (separator != NULL)
		{
			const char* request_header = (const char*)membuf->data;
			*separator = '\0';
			payload = separator + 4;

			if (request_header[0] == 'G' && request_header[1] == 'E' && request_header[2] == 'T')
			{
				const char* path_info = request_header + 3;

				while (isspace(*path_info))
					path_info++;

				end = (char*)strchr(path_info, ' ');

				if (end)
				{
					*end = '\0';
					request_header = end + 1;
				}

				query_stirng = (char*)strchr(path_info, '?');

				if (query_stirng)
				{
					*query_stirng = '\0';
					query_stirng++;
				}

				handle_get(client, request_header, path_info, query_stirng);
			}
			else if (request_header[0] == 'P' && request_header[1] == 'O' && request_header[2] == 'S' && request_header[3] == 'T')
			{
				const char* path_info = request_header + 4;

				while (isspace(*path_info)) path_info++;
				end = (char*)strchr(path_info, ' ');

				if (end)
				{
					*end = '\0';
					request_header = end + 1;
				}

				handle_post(client, request_header, path_info, payload);

			}
			else
			{
				close_client(client);
			}
		}
	}
	else if (nread == -1)
	{
		close_client(client);
	}

	if (buf->base)
	{
		free(buf->base);
	}
}

static void on_connection_cb(uv_stream_t* server, int status)
{
	assert(server == (uv_stream_t*)&_server);
	if (status == 0)
	{
		uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
		client->data = malloc(sizeof(membuf_t));
		membuf_init((membuf_t*)client->data, 128);
		uv_tcp_init(uv_default_loop(), client);
		uv_accept((uv_stream_t*)&_server, (uv_stream_t*)client);
		uv_read_start((uv_stream_t*)client, on_uv_alloc, on_uv_read);
	}
}

int web_par(char* router_info, char* path_info)
{
	char* p1 = router_info + 1;
	char* p2 = path_info + 1;

	for (;;)
	{
		if (*p1 != '*')
		{
			if (*p1 != *p2)
			{
				p1 = NULL;
				p2 = NULL;
				return 0;
				break;
			}
			else
			{
				if (*p1 == '\0')
				{
					p1 = NULL;
					p2 = NULL;
					return 1;
					break;
				}
				else
				{
					p1++;
					p2++;
				}
			}
		}
		else
		{
			p1++;

			if (*p1 == '\0')
			{
				p1 = NULL;
				p2 = NULL;
				return 1;
				break;
			}
			else
			{
				for (; *p2 != '/'; p2++)
				{
				}
			}
		}
	}

}

char* web_header_parser(char* http_header, char* key)
{
	char* begin = strstr(http_header, key);
	if (begin)
	{
		begin = begin + strlen(key);
		begin++;
		while (isspace(*begin))
			begin++;
		char* end;
		for (end = begin + 1; *end != '\r'; end++)
		{
		}
		*end = '\0';
		return begin;
	}
	else
	{
		return("");
	}
}

char* web_path_parser(char* path, int i)
{
	char* begin = path;
	char* end;
	int n = 0;

	for (; ;)
	{
		if (*begin == '/')
		{
			if (n == i)
			{
				begin++;
				end = begin;
				i = 0;
			}
			else
			{
				begin++;
				n++;
			}

		}
		else if (i == 0)
		{
			if (*end == '/')
			{
				*end = '\0';
				break;
			}
			else if (*end == '\0')
			{
				break;
			}
			else
			{
				end++;
			}
		}
		else
		{
			if (*begin == '\0')
			{
				begin = "";
				break;
			}
			else
			{
				begin++;
			}
		}
	}

	return begin;
}