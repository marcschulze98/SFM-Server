#include "SFM_Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

static int init_connection(void);
char* get_info(struct sockaddr *sa, socklen_t salen);

void* syncreceiveserver_thread_func(void* arg)
{
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;
	int new_fd;
	char* hostname;

	int socket_fd = init_connection();

	
	while(true)
	{
		new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
		hostname = get_info((struct sockaddr*)&their_addr, addr_size);
		printf("%s connected\n", hostname);
		close(new_fd);
	}
	
	pthread_exit(0);
}

static int init_connection(void) //set up socket for accept()
{
	printf("Setting up Syncserver...\n");
	int sockfd;
	int no = 0;
	int getaddrinfo_return;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;			// use IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;		// fill in my IP for me

	if((getaddrinfo_return = getaddrinfo(NULL, "2001", &hints, &res)))
	{
		printf("Error: can't create socket: %s\n", gai_strerror(getaddrinfo_return));
		exit(1);
	}
	if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) //ceate socket
	{
		printf("Error: can't create socket: %s\n", strerror(errno));
		exit(1);
	}
	if(setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)) == -1) //set option to allow IPv6 too
	{
		printf("Error: can't accept IPv4 connection: %s\n", strerror(errno));
		exit(1);
	}
	if(bind(sockfd, res->ai_addr, res->ai_addrlen)) //bind socket
	{
		printf("Error: can't bind to socket: %s\n", strerror(errno));
		exit(1);
	}
	if(listen(sockfd, 3)) //listen to socket
	{
		printf("Error: can't listen on socket: %s\n", strerror(errno));
		exit(1);
	}

	freeaddrinfo(res);
	return sockfd;
}

char* get_info(struct sockaddr *sa, socklen_t salen)
{
	int return_code;
	char* host = malloc(1024);
	
	if((return_code = getnameinfo(sa, salen, host, 1024, NULL, 0, 0)))
	{
		printf("Error: can't get name of host: %s\n", gai_strerror(return_code));
	}
	
	return host;
}
