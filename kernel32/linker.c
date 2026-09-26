/*
 * Copyright (c) 2026 ilizavr
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "linker.h"
#include "shell/fbcon.h"
#include "allocator.h"
#include "vfs.h"

PAK struct function_info
{
	void* call;
	char * name;
	char * description;
	struct function_info* next;
};

volatile struct function_info *head_fnc = 0;

static char *strdup(char *str)
{
	u32 len = strlen(str);
	char * newstr = kalloc(len+1);
	memcpy(newstr,str,len+1);
	return newstr;
}

void* resolve_function(char * function_name)
{
	struct function_info * current = head_fnc;
	while(current)
	{
		if(strcmp(current->name, function_name) == 0) return current->call;
		current=current->next;
	}
	return 0;
}

void register_function(char *function_name, void* call, char *description)
{
	u32 eflags = save_irq();
	struct function_info * newfnc = kalloc(sizeof(struct function_info));
	newfnc->next = head_fnc;
	newfnc->name = strdup(function_name);

	if(description)newfnc->description = strdup(description);
	else newfnc->description = "";

	newfnc->call = call;
	head_fnc = newfnc;
	restore_irq(eflags);
}

bool load_mod(char diskletter,char *name)
{
	struct file* mod = open(diskletter,name);
	if(!mod){LOGE("file %s not found",name);return false;}
	u32 size = mod->getsize(mod);
	if(!size){LOGE("file %s empty",name);return false;}
	void *module = kalloc(size);
	mod->read(mod,module,size,0);

	CALL(module,resolve_function);
}

void help()
{
	struct function_info * current = head_fnc;
	while(current)
	{
		if(current->name[0] == '_') print_color(current->name,0x88FF55);
		else print_color(current->name,0xFFFFFF);
		for(int i = strlen(current->name);i<32;i++)putchar(' ');
		print_color(current->description,0x00FFFF);
		putchar('\n');

		current=current->next;
	}
}
