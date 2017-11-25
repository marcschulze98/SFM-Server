#include "SFM_Server.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void init_users(void);
int init_connection(void);
bool user_valid(const struct string* username, const struct string* password);
struct arguments* create_args(const struct string* username, int new_fd);

struct user* users;
struct linked_list* outgoing_messages = NULL;
uint32_t user_count;
char* this_server_name = "Server1";

int main(int argc, char** argv)
{
	int sockfd, new_fd;
	struct string client_option = { .data = malloc(2), .length = 0, .capacity = 2 };
	struct string username = { .data = malloc(DEFAULT_NAME_LENGTH), .length = 0, .capacity = DEFAULT_NAME_LENGTH };
	struct string password = { .data = malloc(DEFAULT_NAME_LENGTH), .length = 0, .capacity = DEFAULT_NAME_LENGTH };
	struct sockaddr_storage their_addr;
	struct arguments* args;
	socklen_t addr_size;
	pthread_t listenthread;
	pthread_t writethread;
	pthread_t cleanupthread;

   
	addr_size = sizeof their_addr;
	sockfd = init_connection();
	init_users();
	signal(SIGPIPE, SIG_IGN); //ignore SIGPIPE (handled internally)

	//~ while(true)
	{
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size); // accept an incoming connection
	
		while(!get_message(&client_option, new_fd)); //get login data
		while(!get_message(&username, new_fd));
		while(!get_message(&password, new_fd));

		if(user_valid(&username, &password)) //check if user is valid
		{
			printf("User logged in: %s\n", username.data);
		} else {
			printf("User failed login: %s\n", username.data);
		}
		
		args = create_args(&username, new_fd);  //create arguments from connection for the threads
		args->write_exists = false;
		args->listen_exists = false;
		
		for(uint32_t i = 0; i < user_count; i++) //get id (for offset from the user-list)
		{
			if(strcmp((users+i)->name, args->name) == 0)
			{
				args->user_id = i;
				printf("USER-ID = %d\n", args->user_id);
			}
		}

		if(client_option.data[0] == '1' && (users+args->user_id)->listen_connected == false && (users+args->user_id)->write_connected == false) //create thread according to client_option
		{
			args->write_exists = true;
			args->listen_exists = true;
			pthread_create(&listenthread, NULL, listenserver_thread_func, args);
			pthread_create(&writethread, NULL, writeserver_thread_func, args);
		} else if(client_option.data[0] == '2' && (users+args->user_id)->listen_connected == false) {
			args->listen_exists = true;
			pthread_create(&listenthread, NULL, listenserver_thread_func, args);
		} else if(client_option.data[0] == '3' && (users+args->user_id)->write_connected == false) {
			args->write_exists = true;
			pthread_create(&writethread, NULL, writeserver_thread_func, args);
		} else {
			close(new_fd);
		}
		args->writeserver = writethread;
		args->listenserver = listenthread;
		pthread_create(&cleanupthread, NULL, cleanupserver_thread_func, args); //activate cleanupthread for write- and listenthread
		
			
		reset_string(&client_option, 1); //reset sizes in case they got bigger to save memory
		reset_string(&username, DEFAULT_NAME_LENGTH);
		reset_string(&password, DEFAULT_NAME_LENGTH);
	}
	pthread_join(cleanupthread, NULL);

	//cleanup
	close(new_fd);
	destroy_linked_list(users->messages);
	free(client_option.data);
	free(username.data);
	free(password.data);
	free(users);

	return 0;
}

void init_users(void) //gather users from a database
{
	users = malloc(sizeof(*users)*10);
	
	users->name = "dummy";
	users->password = "dummy";
	users->listen_connected = false;
	users->write_connected = false;
	
	(users+1)->name = "test";
	(users+1)->password = "test";
	(users+1)->listen_connected = false;
	(users+1)->write_connected = false;
	
	(users+2)->name = "marc";
	(users+2)->password = "test";
	(users+2)->listen_connected = false;
	(users+2)->write_connected = false;
	(users+2)->messages = malloc(sizeof(*users->messages));
	(users+2)->messages->data = NULL;
	(users+2)->messages->next = NULL;
	pthread_mutex_init(&((users+2)->messages->mutex), NULL);
	
	user_count = 3;

	printf("userinit\n");
}

int init_connection(void) //set up socket for accept()
{
	int sockfd;
	int no = 0;
	struct addrinfo hints, *res;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;			// use IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;		// fill in my IP for me

    getaddrinfo(NULL, "2000", &hints, &res);  // make a socket, bind it, and listen on it
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)))
	{
		printf("Fehler: Kann keine IPV4-Adressen annehmen: %s\n", strerror(errno));
	}
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    listen(sockfd, 3);
    
    freeaddrinfo(res);
    
	return sockfd;
}


bool user_valid(const struct string* username, const struct string* password) //check if user exists in Database
{
	bool found = false;
	for(uint32_t i = 0; i < user_count; i++)
	{
		if(strcmp((users+i)->name,username->data) == 0)
		{
			found = true;
			break;
		}
	}

	if(found)
	{
		for(uint32_t i = 0; i < user_count; i++)
		{
			if(strcmp((users+i)->password,password->data) == 0)
			{
				return true;
			}
		}
	}
	
	return false;
}

struct arguments* create_args(const struct string* username, int new_fd) //create struct from username and fd
{
	struct arguments* args = malloc(sizeof(*args));
	
	args->name = malloc(username->length);
	memcpy(args->name, username->data, username->length);
	args->socket_fd = new_fd;

	return args;
}
