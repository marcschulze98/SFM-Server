#ifndef SFM_H
#define SFM_H
#include "../SFM_Common_C/SFM.h"

struct user
{
	char* name;
	char* password;
	struct linked_list* messages;
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
	struct linked_list* members;
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
uint32_t get_user_id(char* username);

extern struct user* users;
extern struct linked_list* groups;
extern struct linked_list* outgoing_messages;
extern uint32_t user_count;
extern char* this_server_name;
extern const struct string test_connection;


#endif //SMF_H
