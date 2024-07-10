#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
main(void)
{
	printf("CPU is running for %d ticks\n", uptime());
}
