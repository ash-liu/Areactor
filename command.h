
#ifndef _COMMAND_H
#define _COMMAND_H

#include "client.h"

typedef void CommandProc(struct client *c);

struct command{
	char *name;
	CommandProc *pro;
};


void init_command_table();
struct command *get_command_from_name(const char *name);
struct command *get_command_from_index(int index);
int get_commands_number();


extern struct command command_table[];

#endif

