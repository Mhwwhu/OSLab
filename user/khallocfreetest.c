#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SIZE 100

int
main(void)
{
	void* addrs[SIZE];
	for (int i = 1; i < SIZE; i++) {
		printf("try to allocate for %d bytes\n", i);
		addrs[i] = khalloctest(i);
	}
	printf("allocating success\n");
	for (int i = SIZE - 1; i >= 1; i--) {
		printf("try to free %p\n", addrs[i]);
		khfreetest(addrs[i]);
	}
	// khfreetest(addrs[8]);
	// khfreetest(addrs[6]);
	// khfreetest(addrs[3]);
	// khfreetest(addrs[2]);
	// khfreetest(addrs[9]);
	// khfreetest(addrs[7]);
	// khfreetest(addrs[5]);
	// khfreetest(addrs[4]);
	// khfreetest(addrs[1]);
	// printf("%x\n", khalloctest(10));
	exit(0);
}
