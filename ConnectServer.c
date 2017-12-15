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
struct dynamic_array* groups;
struct dynamic_array* outgoing_messages;
uint32_t user_count;
char* this_server_name = "Server1"; 
const struct string test_connection = { .data = "\x00\x01\0", .length = 3};

int main(int argc, char** argv)
{
	const struct string accept_login = { .data = "\x00\x02" "1\0", .length = 4};
	const struct string deny_login = { .data = "\x00\x02" "0\0", .length = 4};
	int sockfd, new_fd;
	struct string client_option = new_string(2);
	struct string username = new_string(DEFAULT_NAME_LENGTH);
	struct string password = new_string(DEFAULT_NAME_LENGTH);
	struct sockaddr_storage their_addr;
	struct arguments* args;
	socklen_t addr_size;
	pthread_t listenthread;
	pthread_t writethread;
	pthread_t cleanupthread;
	pthread_t syncsendthread;
	pthread_t syncreceivethread;
	groups = new_dynamic_array();
	outgoing_messages = new_dynamic_array();
	
	addr_size = sizeof(their_addr);
	sockfd = init_connection();
	init_users();
	signal(SIGPIPE, SIG_IGN); //ignore SIGPIPE (handled internally)
	
	pthread_create(&syncsendthread, NULL, syncsendserver_thread_func, NULL);
	pthread_detach(syncsendthread);
	pthread_create(&syncreceivethread, NULL, syncreceiveserver_thread_func, NULL);
	pthread_detach(syncreceivethread);
	
	while(true)
	{
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size); // accept an incoming connection
		
		struct return_info return_codes;
		//get login data
		
		do return_codes = get_message(&client_option, new_fd);
		while(!return_codes.return_code); 
		if(return_codes.error_occured)
			goto cleanup;

		do return_codes = get_message(&username, new_fd);
		while(!return_codes.return_code);
		if(return_codes.error_occured)
			goto cleanup;
		
		do return_codes = get_message(&password, new_fd);
		while(!return_codes.return_code);
		if(return_codes.error_occured)
			goto cleanup;

		if(user_valid(&username, &password)) //check if user is valid
		{
			printf("User logged in: %s\n", username.data);
			send_string(&accept_login, new_fd);
		} else {
			printf("User failed login: %s\n", username.data);
			send_string(&deny_login, new_fd);
			goto cleanup;
		}
		
		args = create_args(&username, new_fd);  //create arguments from connection for the threads
		
		for(uint32_t i = 0; i < user_count; i++) //get id (for offset from the user-list)
		{
			if(strcmp((users+i)->name, args->name) == 0)
			{
				args->user_id = (int32_t)i;
				printf("USER-ID = %d\n", args->user_id);
				break;
			}
		}
		bool listen_connected = atomic_load(&(users+args->user_id)->listen_connected);
		bool write_connected = atomic_load(&(users+args->user_id)->write_connected);
		
		if(args->user_id < 0)
			goto cleanup;

		if(client_option.data[0] == '1' &&  listen_connected == false && write_connected == false) //create thread according to client_option
		{
			args->write_exists = true;
			args->listen_exists = true;
			pthread_create(&listenthread, NULL, listenserver_thread_func, args);
			pthread_create(&writethread, NULL, writeserver_thread_func, args);
		} else if(client_option.data[0] == '2' && listen_connected == false) {
			args->listen_exists = true;
			pthread_create(&listenthread, NULL, listenserver_thread_func, args);
		} else if(client_option.data[0] == '3' && write_connected == false) {
			args->write_exists = true;
			pthread_create(&writethread, NULL, writeserver_thread_func, args);
		}
		
		args->writeserver = writethread;
		args->listenserver = listenthread;
		pthread_create(&cleanupthread, NULL, cleanupserver_thread_func, args); //activate cleanupthread for write- and listenthread
		pthread_detach(cleanupthread);
		
	cleanup:
		reset_string(&client_option, 1); //reset sizes in case they got bigger to save memory
		reset_string(&username, DEFAULT_NAME_LENGTH);
		reset_string(&password, DEFAULT_NAME_LENGTH);
	}

	//cleanup
	destroy_dynamic_array(users->messages);
	free(client_option.data);
	free(username.data);
	free(password.data);
	free(users);

	return 0;
}

void init_users(void) //gather users from a database
{
	FILE* file = fopen("userlist", "r");
	if(file == NULL)
	{
		printf("Couldn't open userlist file: %s\n", strerror(errno));
		exit(1);
	}
	uint32_t users_size = 32;
	users = malloc(sizeof(*users)*users_size);
	char username[256];
	char password[256];
	uint32_t i = 0;
	
	while(fscanf(file, "%s %s", username, password) == 2)
	{
		(users+i)->name = malloc(strlen(username)+1);
		strcpy((users+i)->name, username);
		(users+i)->password = malloc(strlen(password)+1);
		strcpy((users+i)->password, password);

		(users+i)->listen_connected = false;
		(users+i)->write_connected = false;
		(users+i)->messages = new_dynamic_array();
	
		i++;	
		if(i >= users_size)
		{
			users_size *= 2;
			users = realloc(users, sizeof(*users)*users_size);
		}
	}

	fclose(file);
	user_count = i;
	printf("userinit\n");
}

int init_connection(void) //set up socket for accept()
{
	printf("Setting up Masterserver...\n");
	int sockfd;
	int no = 0;
	int getaddrinfo_return;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6;			// use IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;		// fill in my IP for me

	if((getaddrinfo_return = getaddrinfo(NULL, "2000", &hints, &res)))
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
	args->user_id = -1;
	args->socket_fd = new_fd;
	args->write_exists = false;
	args->listen_exists = false;

	return args;
}
