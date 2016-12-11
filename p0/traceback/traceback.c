/** @file traceback.c
 *  @brief The traceback function
 *
 *  This file contains the traceback function for the traceback library
 *
 *  @author Harry Q. Bovik (hqbovik)
 *  @bug Unimplemented
 */
#include <string.h>
#include "traceback_internal.h"
extern int read_ebp(void);
/*
 * Search the function information on the table(functions)
 * The table store the function address in order, so compare the addr
 * we could get the specific function that contain the ret_addr
 */
static inline int get_func_idx_from_table(int * ret_addr) {
	const functsym_t * iter = &functions[0];
	for (int i = 0; (iter->name)[0] != '\0' || i == FUNCTS_MAX_NUM; ++i) {
		/* Once the bigger one occur, 
		 * the last one is the function address */
		if ((unsigned int)iter->addr > (unsigned int)ret_addr)
			return i - 1;
		++iter;
	}
	return -1;
}

typedef char boolean;
/* test if the argument list(args[]) is end */
static inline boolean is_arglist_end(const argsym_t * element) {
	return '\0' == (element->name)[0];
}
#include <ctype.h>
/* print the given string with the fixed format
 * 1. No more than 25 characters
 * 2. if the length is greater than 25, print ... as follow
 * 3. let " to include the string.
 */
static void print_string(FILE * fp, const char * print) {
#define MAX_PRINT_LEN 25
	fprintf(fp, "\"");
	/* test if it is a unprintable string */
	int i = 0;
#if 0
	for(i = 0; i < MAX_PRINT_LEN; ++i) {
		if(!isprint(print[i])) {
			fprintf(fp, "0x%p\"", (void *)print);
			return;
		}
	}
#endif
	for(i = 0; i < MAX_PRINT_LEN; ++i) {
		if('\0' == print[i])
			break;
		fprintf(fp, "%c", print[i]);
	}
	/* if string is greater than 25, 
	   than replace the rest of the string into ... */
	if(MAX_PRINT_LEN == i)
		fprintf(fp, "...\"");
	else
		fprintf(fp, "\"");
#undef MAX_PRINT_LEN
}


static void print_string_array(FILE* fp, const char ** print_arr) {
	/* 4-1 means that C-Style array start with 0, 
	   so [0,3) has three element */
#define MAX_PRINT_NUM (4-1)
	fprintf(fp, "{");
	int i = 0;
    for(i = 0; i < MAX_PRINT_NUM; ++i) {
		if(NULL == print_arr[i])
			break;
		if (i != 0)
			fprintf(fp, ", ");
		print_string(fp, print_arr[i]);
	}
	if (MAX_PRINT_NUM == i)
		fprintf(fp, ", ...}");
	else
		fprintf(fp, "}");
#undef MAX_PRINT_NUM
}

enum print_error {
	NONE_OUTPUT     = -1,
	NONE_VALID_IDX  = -2,
	NONE_VALID_ADDR = -3
};

/*
 * print the function's argument in fixed format
 * if the argment is not printable, show the address.
 * Function(arg1=0x1348321)  in
 */
static int print_func_args_to_file(FILE * fp, 
		const argsym_t * arg_list, int * addr) {
    int iter = 0;
	if (is_arglist_end(&arg_list[iter])){
		fprintf(fp, "void");
		return 0;
	}
	/* iterate util all arguments are printed out 
	   OR touch the upper boundary */
	while(0 == is_arglist_end(&arg_list[iter]) 
		&& iter < ARGS_MAX_NUM) 
	{
		if (iter != 0)
			fprintf(fp, ", ");
		const argsym_t * tmp = &arg_list[iter];
		int * arg_value_addr = (int *)((char *)addr + tmp->offset);
		switch(tmp->type) {
			case TYPE_CHAR:
				if (isprint(*((char *)arg_value_addr)))
					fprintf(fp, "char %s=\'%c\'",
							tmp->name, *((char *)arg_value_addr));
				else
					fprintf(fp, "char %s=\\%d", 
							tmp->name, (int)(*(char *)arg_value_addr));
				break;
			case TYPE_INT:
				fprintf(fp, "int %s=%d", tmp->name, *arg_value_addr);
				break;
			case TYPE_FLOAT:
				fprintf(fp, "float %s=%f", 
						tmp->name, *((float*)arg_value_addr));
				break;
			case TYPE_DOUBLE:
				fprintf(fp, "double %s=%f", 
						tmp->name, *((double*)arg_value_addr));
				break;
			case TYPE_STRING:
				fprintf(fp, "char *%s=", tmp->name);
				print_string(fp, (const char *)(*arg_value_addr));
				break;
			case TYPE_STRING_ARRAY:
				fprintf(fp, "char **%s=", tmp->name);
				print_string_array(fp, (const char **)(*arg_value_addr));
				break;
			case TYPE_VOIDSTAR:
				fprintf(fp, "void *%s=0v%x", tmp->name, (*arg_value_addr));
				break;
			/* set default to unknown type */
			default:
			case TYPE_UNKNOWN:
				fprintf(fp, "UNKNOWN %s=0x%p",
						tmp->name, (void *)(*arg_value_addr));
				break;
		}
		++iter;
	}
	return 0;
}

/*
 * print the function's detail in fixed format:
 * Function fun1-name(arg1=xxx, arg2=yyy) in
 * Function fun2-name(void) in
 * ...and so on
 * @index index is the index of the function table.
 * @addr  addr is the address which stay before the argument of the function
 */
static int print_func_to_file(FILE * fp, int index, int * addr) {
	if (NULL == fp) 
		return NONE_OUTPUT;
	if (index < 0)
		return NONE_VALID_IDX;
	if(NULL == addr)
		return NONE_VALID_ADDR;

    const functsym_t * man = &functions[index];
	/* print the function name */
	fprintf(fp, "Function %s(", man->name);
	/* print the function detail */
	print_func_args_to_file(fp, &((man->args)[0]), addr);
    fprintf(fp, ") in\n");
	return 0;
}

void traceback(FILE *fp)
{
	/* the following just makes a sample access to "functions" array. 
	 * note if "functions" is not referenced somewhere outside the 
	 * file that it's declared in, symtabgen won't be able to find
	 * the symbol. So be sure to always do something with functions */

	/* remove this line once you've got real code here */
	/* current function, traceback, ebp getter */
	int * trace_ebp  = (int *)read_ebp();
	/* get the latest function which call traceback's ebp  */
	int * latest_ebp = (int *)(trace_ebp);
	int * ebp = latest_ebp;
	while(1) {
		/* return address is stay before the ebp  */
		int * ret_addr = (int *)*(ebp + 1);
		int index = get_func_idx_from_table(ret_addr);
		if ( index == -1 ) {
			fprintf(fp, "Function %p(...), in\n", (void *)ret_addr);
			continue;
		}

		/* if you wanna to get the current function's arguments
		 * you should first back to its' caller.
		 */
		ebp = (int *)*ebp;
		print_func_to_file(fp, index, ebp);

		/* If reach the main, then end of the call */
		if (strncmp(functions[index].name, "main", 4) == 0)
			break;
	}
}


