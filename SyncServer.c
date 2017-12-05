#include "SFM_Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

static int init_connection(char* target_server);

void* syncserver_thread_func(void* arg)
{
	while(true)
	{
		pthread_mutex_lock(&outgoing_messages->mutex);
		
		struct outgoing* queue  = dynamic_array_at(outgoing_messages,0);
		int socket_fd = init_connection(queue->target_server);
		if(socket_fd != -1)
		{
			if(queue != NULL)
			{
				struct string* message = dynamic_array_at(queue->messages,0);
				convert_string(message);
				
				send_string(message, socket_fd);
				free(message->data);
				dynamic_array_remove(queue->messages,0);
			}
			close(socket_fd);
		} else {
			queue->tries++;
		}
		
		if(queue->tries > 5)
		{
			struct string* tmp;
			for(size_t i = 0; i < queue->messages->length; i++)
			{
				tmp = dynamic_array_at(queue->messages,0);
				free(tmp->data);
				dynamic_array_remove(queue->messages,0);
			}
		}
		
		pthread_mutex_unlock(&outgoing_messages->mutex);
		sleep(5);
	}
	
	pthread_exit(0);
}

static int init_connection(char* target_server)
{
	int socket_fd;
	int status;
	struct addrinfo hints;
	struct addrinfo *res;  				// will point to the results of getaddrinfo()

	memset(&hints, 0, sizeof hints);	// make sure the struct is empty
	hints.ai_family = AF_INET;     		// don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; 	// TCP stream sockets

	if ((status = getaddrinfo(target_server, "2000", &hints, &res)) != 0) //get ip from hostname
	{
		printf("getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}

	if((socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)	//create socket
	{
		printf ("Socket konnte nicht angelegt werden: %s\n", strerror(errno));
		return -1;
	}

	if(connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1)		//connect to server
	{
		printf ("Verbindung mit dem Master-Server konnte nicht hergestellt werden: %s\n", strerror(errno));
		return -1;
	}

	freeaddrinfo(res);

	return socket_fd;
}
