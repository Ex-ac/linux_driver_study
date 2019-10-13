#include <stdio.h>

static void print2(void);

void print(void)
{	
	print2();
	printf("Hello world\n");
}


void print2(void)
{
	printf("Hello world2\n");
}