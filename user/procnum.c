#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
	int n = procnum();
	printf("The number of processes is %d\n", n);
	exit(0);
}
