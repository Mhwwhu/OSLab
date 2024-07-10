#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define SIZE 100
#define MAXELEMSIZE 4 * 16
#define ACCURACY 10000

static uint seed;

void setSeed(uint sd)
{
	seed = sd;
}

uint randomInt(uint n)
{
	uint a = 1103515245;
	uint c = 12345;
	uint m = -1;
	static uint sd;
	static uint x;
	if (sd != seed) {
		sd = seed;
		x = seed;
	}
	x = a * (x + c) % m;
	return x % n;
}


void shuffle(void* arr, uint64 size, uint64 elemSize, uint times)
{
	if (elemSize > MAXELEMSIZE) return;
	char tmp[MAXELEMSIZE];
	for (int i = 0; i < times; i++) {
		int n1 = randomInt(size);
		int n2 = randomInt(size);
		memcpy(tmp, arr + n1 * elemSize, elemSize);
		memcpy(arr + n1 * elemSize, arr + n2 * elemSize, elemSize);
		memcpy(arr + n2 * elemSize, tmp, elemSize);
	}
}

void shufflePtr(void** arr, uint64 size, uint times)
{
	shuffle(arr, size, sizeof(void*), times);
}

void shuffleInt(int* arr, uint64 size, uint times)
{
	shuffle(arr, size, sizeof(int), times);
}

int randomBool(double possibility)
{
	if (possibility < 0 || possibility > 1) return 0;
	int x = randomInt(ACCURACY);
	return x < possibility * ACCURACY;
}

// #define V1

#ifdef V1
int
main(void)
{
	void* addrs[SIZE];
	int bytes[SIZE];
	setSeed(uptime());
	for (int i = 1; i < SIZE; i++) bytes[i] = i;
	shuffleInt(bytes + 1, SIZE - 1, 200);
	for (int i = 1; i < SIZE; i++) {
		printf("try to allocate for %d bytes\n", bytes[i]);
		addrs[i] = khalloctest(bytes[i]);
	}
	printf("allocating success\n");
	shufflePtr(addrs + 1, SIZE - 1, 200);
	for (int i = 1; i < SIZE; i++) {
		printf("try to free %p\n", addrs[i]);
		khfreetest(addrs[i]);
	}
	exit(0);
}
#else
int
main(void)
{
	void* addrs[SIZE];
	int bytes[SIZE];
	setSeed(uptime());
	for (int i = 0; i < SIZE; i++) bytes[i] = i + 1;
	shuffleInt(bytes, SIZE, 200);
	int alloc_n = 0;
	int free_n = 0;
	while (alloc_n < SIZE || free_n < SIZE) {
		int diff_alloc_free = alloc_n - free_n;
		int randombool = 0;
		int freeposi = 10000 * diff_alloc_free / SIZE;
		if (alloc_n == SIZE) freeposi = 10000;
		else if (diff_alloc_free == 0) freeposi = 0;
		if (randomInt(10000) < (int)freeposi) randombool = 1;
		if (!randombool) {
			addrs[alloc_n - free_n] = khalloctest(bytes[alloc_n]);
			// printf("alloc: %d, %p\n", alloc_n, addrs[alloc_n - free_n]);
			alloc_n++;
		}
		else {
			int index = randomInt(alloc_n - free_n);
			// printf("free: %d, %d, %d, %p\n", free_n, index, alloc_n - free_n, addrs[index]);
			khfreetest(addrs[index]);
			free_n++;
			for (int i = index; i < alloc_n - free_n; i++) addrs[i] = addrs[i + 1];
		}
	}
	return 0;
}
#endif
