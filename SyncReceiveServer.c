/**
* @file
* @brief Recieves messages from another Server, checking for authenticity,
* and forwards the messages to local users.
*/
#include "SFM_Server.h"
#include "SFM_Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

static int init_connection(void);
static struct string_info* get_string_info(struct string* message);
char* get_info(struct sockaddr *sa, socklen_t salen);

void* syncreceiveserver_thread_func(void* arg)
{
	struct sockaddr_storage their_addr;
	socklen_t addr_size = sizeof their_addr;
	int new_fd;
	char* hostname;
	struct string message = new_string(DEFAULT_BUFFER_LENGTH);
	struct return_info return_codes;
	struct string_info* info;
	int left;

	int socket_fd = init_connection();
	
	while(true)
	{
		new_fd = accept(socket_fd, (struct sockaddr *)&their_addr, &addr_size);
		hostname = get_info((struct sockaddr*)&their_addr, addr_size);
		printf("%s connected\n", hostname);
		
		do return_codes = get_message(&message, new_fd);
		while(!return_codes.return_code); 
		if(return_codes.error_occured)
			goto cleanup;
			
		left = atoi(message.data);
		for(int i = 0; i < left; i++)
		{
			do return_codes = get_message(&message, new_fd);
			while(!return_codes.return_code); 
			if(return_codes.error_occured)
				goto cleanup;
				
			if(valid_message_format(&message, true))
			{				
				info = get_string_info(&message);
				if(strcmp(info->source_server, hostname) != 0)
				{
					put_message_local(info);
				} else {
					printf("Error: Mismatched Servername\nTold us: %s\nActual: %s\n", info->source_server, hostname);
				}
				
				free(info->source_server);
				free(info->source_user);
				free(info->message->data);
				free(info->message);
				free(info);
			}
		}
		
	cleanup:
		free(hostname);
		reset_string(&message, DEFAULT_BUFFER_LENGTH);
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

static struct string_info* get_string_info(struct string* message)
{
	struct string_info* info = malloc(sizeof(*info));
	info->message = malloc(sizeof(*info->message));
	info->message->data = malloc(DEFAULT_BUFFER_LENGTH);
	info->message->length = 0;
	info->message->capacity = DEFAULT_BUFFER_LENGTH;
	
	uint32_t server_found = 0;
	uint32_t user_found = 0;
	
	for(uint32_t i = 0; i < message->length; i++)
	{
		if(!server_found && message->data[i] == '@')
		{
			info->source_server = malloc(i+1);
			strncpy(info->source_server, message->data, i);
			info->source_server[i] = '\0';
			server_found = i+1;
		} else if(server_found && message->data[i] == ':') {
			info->source_user = malloc(i-server_found+1);
			strncpy(info->source_user, message->data + server_found, i-server_found);
			info->source_user[i-server_found] = '\0';
			user_found = i+1;
			memcpy(&info->timestamp, message->data+user_found, 8);
			break;
		}
	}
	
	server_found = 0;
	
	for(uint32_t i = user_found+9, j = 0; i < message->length; i++, j++)
	{
		if(!server_found && message->data[i] == '@')
		{
			server_found = i+1;
		} else if(server_found && message->data[i] == ':') {
			info->message_begin = j+1;
			string_append(info->message, message->data+user_found+9);		
			info->message->data[info->message->length] = '\0';
			info->message->length++;
			break;
		}
	}
	
	return info;
}
