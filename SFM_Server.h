#ifndef SFM_H
#define SFM_H
#include "../SFM_Common_C/SFM.h"

struct user
{
	char* name;
	char* password;
	struct dynamic_array* messages;
	_Atomic bool write_connected;
	_Atomic bool listen_connected;
};

struct string_info
{
	char* source_server;
	char* source_user;
	int64_t timestamp;
	uint32_t message_begin;
	struct string* message;
};

struct group
{
	char* master;
	char* name;
	struct dynamic_array* members;
};

struct arguments
{
	char* name;
	int socket_fd;
	uint32_t user_id;
	pthread_t writeserver;
	pthread_t listenserver;
	_Atomic bool write_exists;
	_Atomic bool listen_exists;
};

void* listenserver_thread_func(void* arg);
void* writeserver_thread_func(void* arg);
void* cleanupserver_thread_func(void* arg);
char* contains_group(char* name);
char* get_group_user(char* groupname, uint32_t user_number);
uint32_t get_user_id(char* username);

extern struct user* users;						//users on this server
extern struct dynamic_array* groups;			//groups on this server
extern struct dynamic_array* outgoing_messages;	//messages for other Servers
extern uint32_t user_count;						//number of users
extern char* this_server_name;					//self explanatory
extern const struct string test_connection; 	//default message to check if other side is stil connected


#endif //SMF_H
