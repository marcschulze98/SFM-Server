#include "SFM_Server.h"

uint32_t get_user_id(char* username)
{
	if(username == NULL)
	{
		return 0;
	}
	
	for(uint32_t i = 1; i < user_count; i++)
	{
		if(strcmp((users+i)->name, username) == 0)
		{
			return i;
			break;
		}
	}
	
	return 0;
}
