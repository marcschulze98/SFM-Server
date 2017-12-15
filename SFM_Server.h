/** \mainpage SFM-Server
 * 
 * This is the reference server for the SFM API.
 */
/// @file SFM_Server.h
#ifndef SFM_H
#define SFM_H
#include "../SFM_Common_C/SFM.h"

/**
 * @brief Struct containing username, password, array or messages and
 * bools for connected servers.
 */
struct user
{
	char* name;
	char* password;
	///array of struct string*
	struct dynamic_array* messages;
	_Atomic bool write_connected;
	_Atomic bool listen_connected;
};
/**
 * @brief Struct containing additional information to a message recieved
 * from a client.
 */
struct string_info
{
	char* source_server;
	char* source_user;
	int64_t timestamp;
	uint32_t message_begin;
	struct string* message;
};
/**
 * @brief Struct containing groupleader name, groupname and an array of the members.
 */
struct group
{
	char* master;
	char* name;
	///array of char*
	struct dynamic_array* members;
};
/**
 * @brief Struct containing name, socket file descriptor, user-id, pthread_t
 * of write- and listen-server and booleans for which servers are active
 */
struct arguments
{
	char* name;
	int socket_fd;
	int32_t user_id;
	pthread_t writeserver;
	pthread_t listenserver;
	bool write_exists;
	bool listen_exists;
};
/**
 * @brief Struct for a queue of messages for another Server. Contains the
 * servers name, an array of messages and the number of tries to connect to
 * that server.
 */
struct outgoing
{
	char* target_server;
	///array of struct string*
	struct dynamic_array* messages;
	///After a few tries we simply delete the messages.
	uint8_t tries;
};
/// @brief Starts the listen-server thread.
void* listenserver_thread_func(void* arg);
/// @brief Starts the write-server thread.
void* writeserver_thread_func(void* arg);
/// @brief Starts the sending sync-server thread.
void* syncsendserver_thread_func(void* arg);
/// @brief Starts the recieving sync-server thread.
void* syncreceiveserver_thread_func(void* arg);
/// @brief Starts the cleanup thread.
void* cleanupserver_thread_func(void* arg);
/** @brief Checks if @a name contains a group-name.
 *  @returns char* to name of group
 */
char* contains_group(char* name);
/** @brief Gets the @a user-numberth user in @a groupname.
 *  @note The returned char* should be free'd after use.
 * 	@returns char* to username or NULL if user and/or group couldn't be found.
 */ 
char* get_group_user(char* groupname, uint32_t user_number);
/** @brief Puts the message in @a info in the queue of the target user and
 * replaces the message prefix.
 */ 
void put_message_local(const struct string_info* info);
/** @brief Helper function for put_message_local(), should not be called directly.
 */ 
void copy_helper(struct string* message, const void* source, uint32_t length, char insert);
/** @brief Gets the user id of @a username.
 *  @returns user id if user exists, otherwise 0 (dummy user)
 */ 
uint32_t get_user_id(char* username);

///array of users
extern struct user* users;						//users on this server
///dynamic array of groups, contains struct group*
extern struct dynamic_array* groups;			//groups on this server
///dynamic array of outgoing messages, contains struct outgoing*
extern struct dynamic_array* outgoing_messages;	//messages for other Servers
///length of the users array
extern uint32_t user_count;						//number of users
///string containing this servers name
extern char* this_server_name;					//self explanatory
///struct string to be used to probe the connection
extern const struct string test_connection; 	//default message to check if other side is stil connected


#endif //SMF_H
