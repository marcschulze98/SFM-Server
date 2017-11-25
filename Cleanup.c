#include "SFM_Server.h"

void* cleanupserver_thread_func(void* arg)
{
	struct arguments* args = arg;

	if(args->write_exists)
	{
		pthread_join(args->writeserver, NULL);
	}
	if(args->listen_exists)
	{
		pthread_join(args->listenserver, NULL);
	}

	


	printf("cleanup: %s\n", args->name);
	free(args->name);
	free(args);

	pthread_exit(0);
}
