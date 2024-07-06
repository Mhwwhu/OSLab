#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "rbtree.h"

// #define VERSION1

#define HEAPSTART (PHYSTOP - HEAPLEN)

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
printBlocks()
{
	printf("freeblocks are:\n");
	struct Header* p;
	int nbegin = 1;
	acquire(&lock);
	for (p = base->ptr; nbegin || p != base->ptr; p = p->ptr)
	{
		nbegin = 0;
		printf("address: %p , size: %p\n", p, p->size);
	}
	printf("freep: %p\n", freep);
	release(&lock);
}

#else


extern void print_tree(Rbtree*, Rbnode*);

// 1：首次适应 2：循环首次适应 3：最佳适应 4：最坏适应
#define MODE 1

struct blockNode {
	struct blockNode* next;
};

struct {
	struct spinlock lock;
	struct blockNode* freelist;
} blockChain;

static struct spinlock treelock;
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
	//如果页已分配，则将分配块的数量减少1，如果页面为空，则释放该页
	if (*((uint*)PGROUNDDOWN((uint64)ba) + 1) == 0) {
		(*(uint*)PGROUNDDOWN((uint64)ba))--;
		if (*(uint*)PGROUNDDOWN((uint64)ba) == 0) {
			kfree((void*)PGROUNDDOWN((uint64)ba));
		}
	}
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
	else {
		release(&blockChain.lock);
		return 0;
	}
	release(&blockChain.lock);
	(*(uint*)PGROUNDDOWN((uint64)node))++;
	return (void*)node;
}

void
initpage(void* page_start)
{
	if (page_start == 0) return;
	// 首节点存放页内已分配block的数量和是否为新分配的页
	*((uint*)page_start) = 0;
	*(((uint*)page_start) + 1) = 1;
	char* p;
	// 将第一个block空出来存放页首节点
	for (p = page_start + sizeof(Rbnode); p + sizeof(Rbnode) - (char*)page_start <= PGSIZE; p += sizeof(Rbnode))
		blkfree(p);
	*(((uint*)page_start) + 1) = 0;
}

void
blkinit()
{
	initlock(&blockChain.lock, "blockChain");
	initlock(&treelock, "treelock");
	initpage(kalloc());
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
	if (nbytes == 0) return 0;
	acquire(&treelock);
	Rbnode* min = getmin(&tree_addr, tree_addr.root);
	for (Rbnode* nd = min;; nd = step(&tree_addr, nd)) {
		// 如果当前节点为nil，说明没有可分配的块，分配失败
		if (nd == tree_addr.nil) {
			release(&treelock);
			return 0;
		}
		if (!nd->is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd->size > nbytes) {
			Rbnode* nd_size = (Rbnode*)remove_node(&tree_size, find_node(&tree_size, tree_size.root, nd));
			nd_size->size -= nbytes;
			insert_node(&tree_size, nd_size);
			nd->size -= nbytes;

			// 插入新增的已分配块节点
			void* node_addr_addr = blkalloc();
			void* node_addr_size = blkalloc();
			// 如果获取不到新的节点地址，则尝试申请更多页
			if (!node_addr_addr || !node_addr_size) {
				initpage(kalloc());
				node_addr_addr = blkalloc();
				node_addr_size = blkalloc();
				// 如果获取不到新的页，则分配失败
				if (!node_addr_addr || !node_addr_size) {
					release(&treelock);
					return 0;
				}
			}
			void* ret = OFFTOADDR(nd->addr) + nd->size;
			Rbnode* newNode_addr = init_node(&tree_addr, node_addr_addr, ret, nbytes, 0);
			Rbnode* newNode_size = init_node(&tree_size, node_addr_size, ret, nbytes, 0);
			insert_node(&tree_addr, newNode_addr);
			insert_node(&tree_size, newNode_size);
			release(&treelock);
			return ret;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd->size == nbytes) {
			nd->is_free = 0;
			void* ret = OFFTOADDR(nd->addr);
			release(&treelock);
			return ret;
		}
	}
	// 理论上不执行
	return 0;
}
#endif

void
khfree(void* pa)
{
	if ((uint64)pa < HEAPSTART || (uint64)pa >= PHYSTOP) panic("khfree");
	acquire(&treelock);
	Rbnode objNode;
	objNode.addr = (uint64)pa - HEAPSTART;
	Rbnode* rmNode = find_node(&tree_addr, tree_addr.root, &objNode);
	// 保证free的地址一定是分配出去的地址
	if (rmNode == tree_addr.nil || rmNode->is_free) {
		release(&treelock);
		panic("khfree");
	}
	rmNode->is_free = 1;
	// 检查前后节点能否合并
	Rbnode* prev = step_back(&tree_addr, rmNode);
	Rbnode* next = step(&tree_addr, rmNode);
	int integrate_next = 0;
	int integrate_prev = 0;
	Rbnode* rmnd_size = find_node(&tree_size, tree_size.root, rmNode);
	Rbnode* pvnd_size = find_node(&tree_size, tree_size.root, prev);
	// 后节点能合并
	if (next != tree_addr.nil && rmNode->addr + rmNode->size == next->addr && next->is_free) {
		remove_node(&tree_size, rmnd_size);
		blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, next)));
		rmnd_size->size += next->size;

		blkfree(remove_node(&tree_addr, next));
		rmNode->size += next->size;

		integrate_next = 1;
	}
	// 前节点能合并
	if (prev != tree_addr.nil && prev->addr + prev->size == rmNode->addr && prev->is_free) {
		blkfree(remove_node(&tree_addr, rmNode));
		prev->size += rmNode->size;

		remove_node(&tree_size, pvnd_size);
		// 防止重复删除
		if (!integrate_next) {
			remove_node(&tree_size, rmnd_size);
		}
		pvnd_size->size += rmnd_size->size;
		insert_node(&tree_size, pvnd_size);
		blkfree(rmnd_size);

		integrate_prev = 1;
	}
	if (integrate_next && !integrate_prev) {
		insert_node(&tree_size, rmnd_size);
	}
	release(&treelock);
}

void printBlocks()
{
	// printf("blocks are:\n");
	// Rbnode* nd;
	// acquire(&treelock);
	// for (nd = getmin(&tree_addr, tree_addr.root); nd != tree_addr.nil; nd = step(&tree_addr, nd))
	// {
	// 	printf("address: %p , size: 0x%x, is free: %s\n", OFFTOADDR(nd->addr), nd->size, nd->is_free ? "true" : "false");
	// }
	// release(&treelock);
	// print_tree(&tree_addr, tree_addr.root);
	// printf("***\n");
}

#endif
