#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

extern char end[]; // first address after kernel.
// defined by kernel.ld.

// 24 Bytes, 170 freelists per page
// typedef struct {
// 	freelist* next;
// 	uint64 sz;
// }freelist;

// freelist* pfree;

// struct {
// 	struct spinlock lock;
// 	freelist* free;
// } khmem;

// void
// khint()
// {
// 	initlock(&khmem.lock, "khmem");
// 	khmem.free = PHYSTOP - HEAPLEN;
// 	khmem.free->sz = HEAPLEN - sizeof(freelist);
// 	khmem.free->next = khmem.free;
// 	release(&khmem.lock);
// }

// void*
// khalloc(uint64 nbytes)
// {
// 	void* addr;
// 	if (nbytes < khmem.free->sz)
// 	{
// 		addr = (void*)(khmem.free + 1);

// 	}
// }

struct Header {
	struct Header* ptr;
	uint size;
};

static struct Header* base;
static struct Header* freep;

void
khinit()
{
	base = (struct Header*)(PHYSTOP - HEAPLEN);
	base->size = HEAPLEN;
	base->ptr = base;
	freep = base;
}


void
khfree(void* ap)
{
	if (ap < (void*)base || ap >= (void*)PHYSTOP) panic("khfree");

	struct Header* bp, * p;

	bp = (struct Header*)ap - 1;
	if (freep == 0) {
		bp->ptr = bp;
		freep = bp;
		return;
	}
	for (p = freep; !(bp > p && bp < p->ptr); p = p->ptr)
		if (p >= p->ptr && (bp > p || bp < p->ptr))
			break;
	if ((void*)bp + bp->size == p->ptr) {
		bp->size += p->ptr->size;
		bp->ptr = p->ptr->ptr;
	}
	else
		bp->ptr = p->ptr;
	if ((void*)p + p->size == bp) {
		p->size += bp->size;
		p->ptr = bp->ptr;
	}
	else
		p->ptr = bp;
	freep = p;
}

void*
khalloc(uint nbytes)
{
	struct Header* p, * prevp;

	if ((prevp = freep) == 0) {
		panic("khalloc");
	}
	int totalBytes = sizeof(struct Header) + nbytes;
	for (p = prevp->ptr; ; prevp = p, p = p->ptr) {
		if (p->size >= totalBytes) {
			if (p->size == totalBytes) {
				// only remains 1 free block
				if (p->ptr == p) {
					prevp = 0;
					p->size = 0;
				}
				else
					prevp->ptr = p->ptr;
			}
			else {
				p->size -= totalBytes;
				p = (struct Header*)((void*)p + p->size);
				p->size = totalBytes;
			}
			freep = prevp;
			return (void*)(p + 1);
		}
		if (p == freep)
			panic("khalloc");
	}
}

void
printFreeBlock()
{
	printf("%p\n", PHYSTOP);
	printf("%p\n", HEAPLEN);
	printf("freeblocks are:\n");
	struct Header* p;
	int nbegin = 1;
	for (p = base->ptr; nbegin || p != base->ptr; p = p->ptr)
	{
		nbegin = 0;
		printf("address: %p , size: 0x%x\n", p, p->size);
	}
}
