#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
	for (int i = 0; i < 200; i++) {
		khalloctest(1);
	}
	exit(0);
}
