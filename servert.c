#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>


#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

char webext[30][1024];
char mediatype[30][1024];
char defaultPage[3][1024];
int numLoops;
int port;
char *root;

struct clientSock
{
	int *sockets;
};

char **getTypes(char stringToSplit[])
{
	//splits up string based on spaces
	char **type = malloc(2*sizeof *type);
	const char d[] = " ";
	char *token;
	token = strtok(stringToSplit, d);
	int i = 0;
	while(token != NULL)
	{
		type[i] = token;
		token = strtok(NULL, d);
		i++;
	}
	return type;
}

void parseConf()
{
	//parses the ws.conf file
	char findPort[] = "#Port\n";
	char findRoot[] = "#DocumentRoot\n";
	char findDef[] = "#Default Web Page\n";
	char findTypes[] = "#Content Types\n";
	char* lis[4];
	lis[0] = strdup(findPort);
	lis[1] = strdup(findRoot);
	lis[2] = strdup(findDef);
	lis[3] = strdup(findTypes);

	int pInd, rInd, dInd, tInd;
	int i = 0;
	int j = 0;
	int numLines = 0;
	char line[256];
	char* lines[20];
	char *pos;
	char **types;

	FILE *fp = fopen("ws.conf", "r+");
	if(fp == NULL)
	{
		printf("error");
		exit(-1);
	}

	while(fgets(line, sizeof(line), fp) != NULL)
	{
		lines[j] = strdup(line);
		j++;
		numLines++;
	}
	fclose(fp);

	for(i = 0; i < numLines; i++)
	{
		if(strcmp(lines[i],lis[0]) == 0)
		{
			pInd = i + 1;
		}
		if(strcmp(lines[i],lis[1]) == 0)
		{
			rInd = i + 1;
		}
		if(strcmp(lines[i],lis[2]) == 0)
		{
			dInd = i + 1;
		}
		if(strcmp(lines[i],lis[3]) == 0)
		{
			tInd = i + 1;
		}
	}

	if((numLines - tInd) > 30)
	{
		printf("Error: Too many file type in ws.conf");
		exit(-1);
	}

	port = atoi(lines[pInd]);
	root = lines[rInd];
	if((pos = strchr(root, '\n')) != NULL)
	{
		*pos = '\0';
	}

	j = 0;
	for(i = dInd; i < tInd - 1; i++)
	{
		strcpy(defaultPage[j], lines[i]);
		if((pos = strchr(defaultPage[j], '\n')) != NULL)
		{
			*pos = '\0';
		}
		j++;
	}

	j = 0;
	for(i = tInd; i < numLines; i++)
	{
		types = getTypes(lines[i]);
		strcpy(webext[j],types[0]);
		strcpy(mediatype[j], types[1]);
		if((pos = strchr(mediatype[j], '\n')) != NULL)
		{
			*pos = '\0';
		}
		j++;
		numLoops++;
	}
	
}

int fileSize(int fd)
{
	//grabs the file size (allows for sending files)
	struct stat stat_struct;
	if (fstat(fd, &stat_struct) == -1)
		return (-1);
		
	return (int) stat_struct.st_size;
}

void sendMsg(int fd, char *msg)
{
	//sends a message to the client
	int size = strlen(msg);
	int numBytesSent;
	printf("Size of header: %d\n", size);
	do
	{
		printf("Header:\n%s\n", msg);
		numBytesSent = send(fd, msg, size, MSG_NOSIGNAL);
		printf("Bytes Sent: %d\n", numBytesSent);
		if(numBytesSent <= 0) break;
		size -= numBytesSent;
		msg += numBytesSent;
	}while(size > 0);
}
int uriCheck(char *request, int fd)
{
	//checks to make sure the uri requested is valid
	char *req = request;
	char temp[500];
	char uri[500];
	char *p1, *p2;
	p2 = strstr(req, " HTTP");
	p1 = strchr(req, '/');
	int n = p2 - p1;
	strncpy(temp, request + 4, n);
	strcpy(uri, temp);
	if(strlen(uri) == 0)
	{
		strcat(uri, "/index.html");
	}
	char *chr = uri;
	printf("URI: %s", uri);
	char *allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=";
	while(*chr)
	{
		if(strchr(allowed, *chr) == NULL)
		{
			char str[500];
			printf("HTTP/1.1 400 Bad Request: Invalid Method: %s\n", uri);
			sprintf(str, "HTTP/1.1 400 Bad Request: Invalid URI: %s\rnContent-Type: text/html\r\n\r\n", uri);
			sendMsg(fd, str);
			sendMsg(fd, "<html><head><title>400 Invalid URI</head></title>");
			sendMsg(fd, "<body><p>400 Unsuported URI</p></body></html>");
			return -1;
		}
		chr++;
	}
	return 1;
}

void send500(int fd)
{
	//sends 500 error if server error occurs
	printf("HTTP/1.1 500 Internal Server Error: cannot allocate memory\n");
	sendMsg(fd, "HTTP/1.1 500 Internal Server Error: cannot allocate memory\r\n");
}

int methodCheck(char *request, int fd)
{
	//checks to make sure it is a GET request
	char *req = request;
	char str[1024];
	if(strncmp(req, "GET ", 4) == 0)
	{
		return 1;
	}
	else
	{
		int n;
		char method[1024];
		n = strchr(req, '/') - req;
		strncpy(method, req, n);
		strncpy(method, method, strlen(method) - 1);
		printf("HTTP/1.1 400 Bad Request: Invalid Method: %s\n", method);
		sprintf(str, "HTTP/1.1 400 Bad Request: Invalid Method: %s\rnContent-Type: text/html\r\n\r\n", method);
		sendMsg(fd, str);
		sendMsg(fd, "<html><head><title>400 Invalid Method</head></title>");
		sendMsg(fd, "<body><p>400 Unsuported Method Type</p></body></html>");
		return -1;
	}
	
	return 1;
}

int check501(char *resource, int fd)
{
	//checks to make sure the file extension is implemented
	char *res = resource; 
	char *fileExten = strchr(res, '.');
	int i;
	for(i = 0; i < numLoops; i++)
	{
		if(strcmp(fileExten, webext[i]) == 0) break;
	}
	if(i == numLoops)
	{
		char str[500];
		printf("HTTP/1.1 501 Not Implemented: %s\n", resource);
		sprintf(str, "HTTP/1.1 501 Not Implemented: %s\r\n\r\n", resource);
		sendMsg(fd, str);
		sendMsg(fd, "<html><head><title>501 Not Implemented:</head></title>");
		sendMsg(fd, "<body><p>501 File Type Is Not Implemented</p></body></html>");
		return -1;
	}
	return 1;
}

int httpcheck(char *request, int fd)
{
	//checks to make sure http version is 1.1 or 1.0
	char *ptr;
	char *ptr2;
	char *req = request;
	int check = 0;
	puts(req);
	ptr = strstr(req, "HTTP/1.1");
	if(ptr == NULL)
	{
		check = 1;
	}
	if(check == 1)
	{
		ptr2 = strstr(req, "HTTP/1.0");
		if(ptr2 == NULL)
		{
			char *p;
			char httpVersion[1024];
			char str[500];
			p = strstr(req, "HTTP/");
			p += 5;
			strncpy(httpVersion, p, 3);
			printf("HTTP/1.1 400 Bad Request: Invalid HTTP-Version: %s\n", httpVersion);
			sprintf(str, "HTTP/1.1 400 Bad Request: Invalid HTTP-Version: %s\rnContent-Type: text/html\r\n\r\n", httpVersion);
			sendMsg(fd, str);
			sendMsg(fd, "<html><head><title>400 Invalid HTTP-Version</head></title>");
			sendMsg(fd, "<body><p>400 Unsuported HTTP Version</p></body></html>");
			return -1;
		}
	}
	return 1;
}

void con(void *args)
{
	//set up variables
	struct clientSock* arguments = args;
	int* temp = arguments->sockets;
	int fd = *temp;
	char request[8192], resource[500], *ptr;
	unsigned char buff[1024];
	int resourcefd, length, keepAlive;
	FILE *page;
	time_t lastReq = 0;
	recv(fd, request, 8102, 0);
	do
	{
		//receive request
		char cp[8192];
		while(recv(fd, cp, 8192, MSG_DONTWAIT) > 0)
		{
			int i;
			strcat(request, cp);
			for(i = 0; i < 8192; i++)
			{
				cp[i] = 0;
			}
		} 
		printf("Request: %s\n", request);
		if(strlen(request) > 0)
		{
			//set time since last request
			lastReq = time(NULL);
			//check to see if keep-alive flag was sent
			char *keep = strstr(request, "keep");
			if(keep == NULL)
			{
				keepAlive = 0;
			}
			else keepAlive = 1;

			//check to makge sure it is an http request
			ptr = strstr(request, " HTTP/");
			if(ptr == NULL)
			{
				printf("Not HTTP Request\n");
			}
			else
			{
				//check if http version, method, and uri is okay	
				int versionOk = httpcheck(request, fd);
				int methodOk = methodCheck(request, fd);
				int uriOk = uriCheck(request, fd);
				if(versionOk == 1 && methodOk == 1 && uriOk == 1);
				{
					*ptr = 0;
					ptr = NULL;
					ptr = request + 4;
					//check to see if index.html needs to be appended
					if(ptr[strlen(ptr) - 1] == '/')
					{
						strcat(ptr, "index.html");
					}
					//place the recource onto the root directory
					strcpy(resource, root);
					strcat(resource, ptr);
					//check if resource is available
					int ok501 = check501(resource, fd);
					if(ok501 == -1) break;
					
					printf("File: %s\n", resource);

					//open file and send it
					char *fileExten = strchr(request, '.');
					int i;
					for(i = 0; i < numLoops; i++)
					{
						if(strcmp(fileExten, webext[i]) == 0)
						{
							resourcefd = open(resource, O_RDONLY, 0);
							//check for file not found
							if(resourcefd < 0)
							{
								char str[2048];
								printf("HTTP/1.1 404 Not Found: %s\n", resource);
								sprintf(str, "HTTP/1.1 404 Not Found: %s\r\nContent-Type: text/html\r\n\r\n", resource);
								sendMsg(fd, str);
								sendMsg(fd, "<html><head><title>404 Not Found</head></title>");
								sendMsg(fd, "<body><p>404 Not Found: Oops! The request could not find the file</p></body></html>");
							}
							else
							{
								if((length = fileSize(resourcefd)) == -1)
								{
									printf("Error getting File Size");
								}
								close(resourcefd);
								
								printf("200 OK, Content-Type: %s\n\n", mediatype[i]);
								char str[1024];
								sprintf(str, "HTTP/1.1 200 OK \r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", mediatype[i], length);
								sendMsg(fd, str);
								//send file
								page = fopen(resource, "r");
								while(!feof(page))
								{
									size_t bytesToSend;
									bytesToSend = fread(buff, sizeof(unsigned char), 1024, page);
									unsigned char *line = buff;
									ssize_t numBytesSent;
									//printf("Size of message: %zu\n", bytesToSend);
									do
									{
										//printf("Message:\n%s\n", line);
										numBytesSent = send(fd, line, bytesToSend, MSG_NOSIGNAL);
										//printf("Num Bytes sent: %zd\n", numBytesSent);
										if(numBytesSent <= 0) break;
										bytesToSend -= numBytesSent;
										//printf("Bytes left to send: %zu\n", bytesToSend);
										line += numBytesSent;
									}while(bytesToSend > 0);
								}	
							}
						}
					}
				}
			}
		}
	//check for keep alive flag and time out	
	}while((keepAlive == 1) && (((long long) (time(NULL) - lastReq)) < 10)); 

	close(fd);
	free(arguments);
	pthread_exit((void *)0);
}

int main(int argc, char *argv[])
{
	//parse ws.conf and set up variables
	parseConf();
	int sock;
	int listener;
	struct sockaddr_in cli_addr;
	struct sockaddr_in serv_addr;
	socklen_t cli_len = sizeof(cli_addr);
	//make server socket
	if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		perror("Error on socket creation");
		exit(-1);
	}
	//zero adn set attributes of server struct
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	//bind the socket
	if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
	{
		send500(sock);
		perror("Error on bind");
		exit(-1);
	}
	//listen for connections on the server socket
	if(listen(sock, 5) == -1)
	{
		send500(sock);
		perror("Error on listen");
		exit(-1);
	}
	while(1)
	{
		//accept and make client socket
		struct clientSock *clisocket = malloc(sizeof(struct clientSock));
		listener = accept(sock, (struct sockaddr*)&cli_addr, &cli_len);
		clisocket->sockets = &listener;
		printf("got connection\n");
		if(listener == -1)
		{
			send500(sock);
			perror("Error on accept");
		}
		//allow for concurrent connections using pthreads
		pthread_t tid;
		int rc;
		rc = pthread_create(&tid, NULL,(void *) con, (void *) clisocket);
		if(rc)
		{
			send500(sock);
			perror("Error in pthread_create");
			close(sock);
			exit(EXIT_FAILURE);
		}
	}
	close(sock);
	return 0;
}


