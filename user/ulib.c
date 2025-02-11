#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

//
// wrapper so that it's OK if main() does not call exit().
//
void
_main()
{
	extern int main();
	main();
	exit(0);
}

char*
strcpy(char* s, const char* t)
{
	char* os;

	os = s;
	while ((*s++ = *t++) != 0)
		;
	return os;
}

int
strcmp(const char* p, const char* q)
{
	while (*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}

uint
strlen(const char* s)
{
	int n;

	for (n = 0; s[n]; n++)
		;
	return n;
}

void*
memset(void* dst, int c, uint n)
{
	char* cdst = (char*)dst;
	int i;
	for (i = 0; i < n; i++) {
		cdst[i] = c;
	}
	return dst;
}

char*
strchr(const char* s, char c)
{
	for (; *s; s++)
		if (*s == c)
			return (char*)s;
	return 0;
}

char*
gets(char* buf, int max)
{
	int i, cc;
	char c;

	for (i = 0; i + 1 < max; ) {
		cc = read(0, &c, 1);
		if (cc < 1)
			break;
		buf[i++] = c;
		if (c == '\n' || c == '\r')
			break;
	}
	buf[i] = '\0';
	return buf;
}

int
stat(const char* n, struct stat* st)
{
	int fd;
	int r;

	fd = open(n, O_RDONLY);
	if (fd < 0)
		return -1;
	r = fstat(fd, st);
	close(fd);
	return r;
}

int
atoi(const char* s)
{
	int n;
	int radix;

	n = 0;
	radix = 10;
	if (strlen(s) > 2)
	{
		if (s[0] == '0' && s[1] == 'x')
		{
			radix = 16;
			s += 2;
		}
		else if (s[0] == '0' && s[1] == 'b')
		{
			radix = 2;
			s += 2;
		}
	}
	if (radix == 10) {
		while ('0' <= *s && *s <= '9')
			n = n * radix + *s++ - '0';
	}
	else if (radix == 2)
	{
		while ('0' <= *s && *s <= '1')
			n = n * radix + *s++ - '0';
	}
	else if (radix == 16)
	{
		while (('0' <= *s && *s <= '9') || ('a' <= *s && *s <= 'f') || ('A' <= *s && *s <= 'F'))
		{
			if ('0' <= *s && *s <= '9') n = n * radix + *s++ - '0';
			else if ('a' <= *s && *s <= 'f') n = n * radix + *s++ - 'a' + 10;
			else n = n * radix + *s++ - 'A' + 10;
		}
	}
	return n;
}

uint64
atol(const char* s)
{
	uint64 n;
	int radix;

	n = 0;
	radix = 10;
	if (strlen(s) > 2)
	{
		if (s[0] == '0' && s[1] == 'x')
		{
			radix = 16;
			s += 2;
		}
		else if (s[0] == '0' && s[1] == 'b')
		{
			radix = 2;
			s += 2;
		}
	}
	if (radix == 10) {
		while ('0' <= *s && *s <= '9')
			n = n * radix + *s++ - '0';
	}
	else if (radix == 2)
	{
		while ('0' <= *s && *s <= '1')
			n = n * radix + *s++ - '0';
	}
	else if (radix == 16)
	{
		while (('0' <= *s && *s <= '9') || ('a' <= *s && *s <= 'f') || ('A' <= *s && *s <= 'F'))
		{
			if ('0' <= *s && *s <= '9') n = n * radix + *s++ - '0';
			else if ('a' <= *s && *s <= 'f') n = n * radix + *s++ - 'a' + 10;
			else n = n * radix + *s++ - 'A' + 10;
		}
	}
	return n;
}

void*
memmove(void* vdst, const void* vsrc, int n)
{
	char* dst;
	const char* src;

	dst = vdst;
	src = vsrc;
	if (src > dst) {
		while (n-- > 0)
			*dst++ = *src++;
	}
	else {
		dst += n;
		src += n;
		while (n-- > 0)
			*--dst = *--src;
	}
	return vdst;
}

int
memcmp(const void* s1, const void* s2, uint n)
{
	const char* p1 = s1, * p2 = s2;
	while (n-- > 0) {
		if (*p1 != *p2) {
			return *p1 - *p2;
		}
		p1++;
		p2++;
	}
	return 0;
}

void*
memcpy(void* dst, const void* src, uint n)
{
	return memmove(dst, src, n);
}
