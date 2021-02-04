#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> 
#include <sys/stat.h>


char* PATH_PREFIX = ".";

char* 404_PAGE = "./404.html"

struct HTTP_Header {
	char* name;
	char* value;
};

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}

void build_response(char* buf, int code, struct HTTP_Header* headers, int num_headers, char* body) {
	int buf_pos = 0;
	strcpy(buf, "HTTP/1.0 ");
	buf_pos += 9;

	switch(code){
		case 200:
			strcpy(buf + buf_pos, "200 OK\n");
			buf_pos += 7;
			break;
		case 400:
			strcpy(buf + buf_pos, "400 Bad Request\n");
			buf_pos += 16;
			break;
		case 404:
			strcpy(buf + buf_pos, "404 Not Found\n");
			buf_pos += 14;
			break;
		case 405:
			strcpy(buf + buf_pos, "405 Method Not Allowed\n");
			buf_pos += 23;
		case 414:
			strcpy(buf + buf_pos, "414 URI Too Long\n");
			buf_pos += 23;
			break;
			break;
		case 500:
		default:
			strcpy(buf + buf_pos, "500 Internal Server Error\n");
			buf_pos += 25;
			break;
	}

	for(int i = 0; i < num_headers; i++) {
		strcpy(headers[i].name, buf + buf_pos);
		buf_pos += strlen(headers[i].name);
		strcpy(buf + buf_pos, ": ");
		buf_pos += 2;
		strcpy(headers[i].value, buf + buf_pos);
		buf_pos += strlen(headers[i].value);
		buf[buf_pos] = '\n';
		buf_pos++;
	}

	buf[buf_pos] = '\n';
	buf_pos++;

	strcpy(buf+buf_pos, body);
	buf_pos += strlen(body);

	buf[buf_pos] = 0;
}

void func(int client_sock) {
	char buffer_in[8192] = {0};
	char buffer_out[8192] = {0};


	read(client_sock, buffer_in, sizeof(buffer_in)); 



	// parse the http

	int buf_pos = 0;
	char method[16];

	char path[2048];

	memset(method, 0 , 16);
	memset(path, 0, 2048);

	// method
	int method_start = buf_pos;
	for(; buf_pos < strlen(buffer_in); buf_pos++) {
		if(buffer_in[buf_pos] == ' ') {
			int method_size = buf_pos - method_start;

			if(method_size >= 16) {
				build_response(buffer_out, 400, NULL, 0 , "");
				write(client_sock, buffer_out, strlen(buffer_out));
				close(client_sock);
				return;
			}
			else strncpy(method, buffer_in + method_start, method_size);

			break;
		}
	}
	//account for space
	buf_pos++;
	// path
	int path_start = buf_pos;
	for(; buf_pos < strlen(buffer_in); buf_pos++) {
		if(buffer_in[buf_pos] == ' ') {
			int path_size = buf_pos - path_start;

			if(path_size >= 2048) {
				build_response(buffer_out, 414, NULL, 0 , "");
				write(client_sock, buffer_out, strlen(buffer_out));
				close(client_sock);
				return;
			}

			else strncpy(path, buffer_in + path_start, path_size);

			break;
		}
	}

	//protocol // since we dont need to interpret this, we are skipping over it.
	for(; buf_pos <  strlen(buffer_in); buf_pos++) {
		if(buffer_in[buf_pos] == '\n') break;
	}

	// TODO: parse headers and body

	printf("New request, Method: %s, Path: %s\n", method, path);

	if(strcmp(method, "GET") != 0) {
		build_response(buffer_out, 405, NULL, 0, "");
		write(client_sock, buffer_out,strlen(buffer_out));
		close(client_sock);
		return;
	}

	// next add path_prefix to front of path

	if(strlen(PATH_PREFIX) + strlen(path) > 2048) {
		build_response(buffer_out, 414, NULL, 0 , "");
		write(client_sock, buffer_out, strlen(buffer_out));
		close(client_sock);
		return;
	}

	for(int i=strlen(path) - 1; i >= 0; i--){
		path[i + strlen(PATH_PREFIX)] = path[i];
	}

	memcpy(path, PATH_PREFIX, strlen(PATH_PREFIX));


	char realpath_result[4096];

	if(realpath(path, realpath_result) == NULL){

		perror("realpath");
		build_response(buffer_out, 404, NULL,0,"404");
		write(client_sock, buffer_out, strlen(buffer_out));
		close(client_sock);
		return;
	}

	if(isDirectory(realpath_result)) {
		strcpy(realpath_result + strlen(realpath_result), "/index.html");
	}

	printf("Loading file %s\n", realpath_result);

	if(access(realpath_result, R_OK) == 0) {



		FILE* fp = fopen(realpath_result, "r");

		//file size
		fseek(fp, 0L, SEEK_END);
		int size = ftell(fp);

		char buffer_out_bigger[size + 2048];
		char buffer_file_in[size + 1];


		rewind(fp);

		fread(buffer_file_in, 1, size, fp);

		buffer_file_in[size] = 0;

		build_response(buffer_out_bigger,200,NULL,0,buffer_file_in);
		write(client_sock, buffer_out_bigger, strlen(buffer_out_bigger));
		close(client_sock);
		return;
	} else {
		perror("access");


		build_response(buffer_out, 404, NULL,0,"");
		write(client_sock, buffer_out, strlen(buffer_out));
		close(client_sock);
		return;
	};

	build_response(buffer_out,500,NULL,0,"");
	write(client_sock, buffer_out, strlen(buffer_out));
	close(client_sock);
	return;
}

int main(void)
{

	printf("test\n");
	int opt = 0;


	int server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if(server_fd == 0){
		perror("socket");
		exit(1);
	}


	if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("socket options");
		exit(1);
	}

	struct sockaddr_in address; 

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);

	if(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
		perror("bind");
		exit(1);
	}

	if(listen(server_fd, 32) < 0){
		perror("listen");
		exit(1);
	}

	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	while(1) {
		int client_sock = accept(server_fd, (struct sockaddr*) &client_addr, &client_addr_len);

		if(client_sock < 0) {
			fprintf(stderr, "server accept failed\n");
		} else {
			func(client_sock);
		}
		sleep(1);
	}
}