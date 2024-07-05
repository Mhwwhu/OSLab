#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "rbtree.h"

#define VERSION1

#define HEAPSTART PHYSTOP - HEAPLEN

#define OFFTOADDR(off) ((void*)((off) + HEAPSTART))

#ifdef VERSION1

struct Header {
	struct Header* ptr;
	uint64 size;
};

static struct spinlock lock;

static struct Header* base;
static struct Header* freep;

void
khinit()
{
	base = (struct Header*)(PHYSTOP - HEAPLEN);
	base->size = HEAPLEN;
	base->ptr = base;
	freep = base;
	initlock(&lock, "Header");
}

// 有效free地址应该由调用者保证
void
khfree(void* ap)
{
	acquire(&lock);
	if ((uint64)ap < HEAPSTART - sizeof(struct Header) || (uint64)ap >= PHYSTOP) panic("khfree");

	struct Header* bp, * p;

	bp = (struct Header*)ap - 1;

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
	release(&lock);
}

void*
khalloc(uint nbytes)
{
	acquire(&lock);
	struct Header* p, * prevp;

	if ((prevp = freep) == 0) {
		return 0;
	}
	int totalBytes = sizeof(struct Header) + nbytes;
	for (p = prevp->ptr; ; prevp = p, p = p->ptr) {
		if (p->size >= totalBytes) {
			if (p->size <= totalBytes) {
				// only remains 1 free block
				if (p->ptr == p) {
					release(&lock);
					return 0;
				}
				prevp->ptr = p->ptr;
			}
			else {
				if (p->size - totalBytes < sizeof(struct Header)) {
					release(&lock);
					return 0;
				}
				p->size -= totalBytes;
				p = (struct Header*)((void*)p + p->size);
				p->size = totalBytes;
			}
			freep = prevp;
			release(&lock);
			return (void*)(p + 1);
		}
		if (p == freep) {
			release(&lock);
			return 0;
		}
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
	acquire(&lock);
	for (p = base->ptr; nbegin || p != base->ptr; p = p->ptr)
	{
		nbegin = 0;
		printf("address: %p , size: %p\n", p, p->size);
	}
	release(&lock);
}

#else

// 1：首次适应 2：循环首次适应 3：最佳适应 4：最坏适应
#define MODE 1

struct blockNode {
	struct blockNode* next;
};

struct {
	struct spinlock lock;
	struct blockNode* freelist;
} blockChain;

static struct spinlock lock;
static Rbtree tree_addr;
static Rbtree tree_size;

int cmpbyaddr(void* b1, void* b2)
{
	Rbnode* node1 = b1;
	Rbnode* node2 = b2;
	if (node1->addr < node2->addr) return -1;
	else if (node1->addr > node2->addr) return 1;
	return 0;
}

int cmpbysize(void* b1, void* b2)
{
	Rbnode* node1 = b1;
	Rbnode* node2 = b2;
	if (node1->size < node2->size) return -1;
	else if (node1->size > node2->size) return 1;
	return 0;
}

// caller should ensure that ba is valid
void
blkfree(void* ba)
{
	struct blockNode* nd = (struct blockNode*)ba;
	acquire(&blockChain.lock);
	nd->next = blockChain.freelist;
	blockChain.freelist = nd;
	release(&blockChain.lock);
}

void*
blkalloc()
{
	struct blockNode* node;
	acquire(&blockChain.lock);
	node = blockChain.freelist;
	if (node)
	{
		blockChain.freelist = node->next;
	}
	release(&blockChain.lock);
	return (void*)node;
}

void
acquirepage(void* page_start)
{
	if (page_start == 0) return;
	char* p;
	for (p = page_start; p + sizeof(Rbnode) - (char*)page_start <= PGSIZE; p += sizeof(Rbnode))
		blkfree(p);
}

void
blkinit()
{
	initlock(&blockChain.lock, "blockChain");
	acquirepage(kalloc());
}

void
khinit()
{
	blkinit();
	init_rbtree(&tree_addr, blkalloc(), blkalloc(), cmpbyaddr);
	init_rbtree(&tree_size, blkalloc(), blkalloc(), cmpbysize);
}

// 首次适应
#if MODE == 1
void*
khalloc(uint nbytes)
{
	Rbnode* min = getmin(&tree_addr, tree_addr.root);
	Rbnode* max = getmax(&tree_addr, tree_addr.root);
	for (Rbnode* nd = min;; nd = step(&tree_addr, nd)) {
		if (!nd->is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd->size > nbytes) {
			Rbnode* nd_size = (Rbnode*)remove_node(&tree_size, nd);
			nd_size->size -= nbytes;
			insert_node(&tree_size, nd_size);
			nd->size -= nbytes;
			return OFFTOADDR(nd->addr) + nd->size;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd->size == nbytes) {
			if (!nd->is_free)
				remove_node(&tree_size, nd);
			void* ret = OFFTOADDR(nd->addr);
			blkfree(nd);
			return ret;
		}
		if (nd == max) return 0;
	}
	// 理论上不执行
	return 0;
}
#endif

void
khfree(void* pa)
{
	if (pa < HEAPSTART || pa >= PHYSTOP) panic("khfree");
	Rbnode objNode;
	objNode.addr = pa;
	Rbnode* prev = getmin(&tree_addr, &objNode);
	if (prev == tree_addr.nil) {

	}

}

#endif
