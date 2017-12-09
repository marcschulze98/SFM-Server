#include "SFM_Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

static int init_connection(char* target_server);

void* syncsendserver_thread_func(void* arg)
{
	int socket_fd;
	size_t left;
	struct string* message;
	struct string* tmp;
	struct string count = new_string(10);
	struct outgoing* queue;
	
	while(true)
	{
		sleep(30);
		pthread_mutex_lock(&outgoing_messages->mutex);
		
		queue  = dynamic_array_at(outgoing_messages,0);
		
		if(queue != NULL)
		{
			socket_fd = init_connection(queue->target_server);
			if(queue->tries > 5)
			{
				for(size_t i = 0; i < queue->messages->length; i++)
				{
					tmp = dynamic_array_at(queue->messages,0);
					free(tmp->data);
					dynamic_array_remove(queue->messages,0);
				}
				free(queue->target_server);
				destroy_dynamic_array(queue->messages);
				dynamic_array_remove(outgoing_messages, 0);
				
			} else if(socket_fd != -1) {
				left = queue->messages->length;
				sprintf(count.data, "%ld", left);
				count.length = (uint32_t)strlen(count.data)+1;
				convert_string(&count);
				send_string(&count, socket_fd);
				
				for(size_t i = 0; i < left; i++)
				{
					message = dynamic_array_at(queue->messages,0);
					convert_string(message);
					send_string(message, socket_fd);
					
					free(message->data);
					dynamic_array_remove(queue->messages,0);
				}

				free(queue->target_server);
				destroy_dynamic_array(queue->messages);
				dynamic_array_remove(outgoing_messages, 0);

				close(socket_fd);
			} else {
				queue->tries++;
			}
			
			
		}
		pthread_mutex_unlock(&outgoing_messages->mutex);
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

	if((status = getaddrinfo(target_server, "2001", &hints, &res)) != 0) //get ip from hostname
	{
		printf("getaddrinfo error: %s\n", gai_strerror(status));
		return -1;
	}

	if((socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1)	//create socket
	{
		printf ("Socket konnte nicht angelegt werden: %s\n", strerror(errno));
	}

	if(connect(socket_fd, res->ai_addr, res->ai_addrlen) == -1)		//connect to server
	{
		printf ("Verbindung mit dem Master-Server konnte nicht hergestellt werden: %s\n", strerror(errno));
		socket_fd = -1;
	}

	freeaddrinfo(res);

	return socket_fd;
}
