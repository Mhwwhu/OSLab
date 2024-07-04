#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("receive only 1 parameter.\n");
		exit(-1);
	}
	printf("%p\n", khalloctest(atoi(argv[1])));
	exit(0);
}
