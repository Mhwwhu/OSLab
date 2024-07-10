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
	if (ap == 0) return;
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

#define PGHEAD_SIZE (2 * sizeof(struct blockNode) + sizeof(uint))

extern char etext[];
extern void print_tree(Rbtree*, Rbnode*);
extern int check_violation(Rbtree*, Rbnode*);

// 1：首次适应 2：循环首次适应 3：最佳适应 4：最坏适应
#define MODE 2

struct blockNode {
	struct blockNode* next;
};

static struct blockNode pageChain;
static struct spinlock pagelock;
static struct spinlock treelock;
static Rbtree tree_addr;
static Rbtree tree_size;
static int is_initializing;

struct blockNode* page_head(void* pa)
{
	return (struct blockNode*)pa;
}

struct blockNode* page_freelist(void* pa)
{
	return (struct blockNode*)pa + 1;
}

uint* page_allocatedBlocks(void* pa)
{
	return (uint*)(pa + 16);
}

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
	if (node1->size > node2->size) return 1;
	if (node1->addr < node2->addr) return -1;
	if (node1->addr > node2->addr) return 1;
	return 0;
}

// caller should ensure that ba is valid
void
blkfree(void* ba)
{
	// printf("free start\n");
	struct blockNode* nd = (struct blockNode*)ba;
	void* pa = (void*)PGROUNDDOWN((uint64)ba);
	if (!is_initializing)
		acquire(&pagelock);

	nd->next = page_freelist(pa)->next;
	page_freelist(pa)->next = nd;

	if (!is_initializing) {
		(*page_allocatedBlocks(pa))--;
		// 如果为空，且当前不是初始化阶段，则释放该页
		if (!*page_allocatedBlocks(pa)) {
			struct blockNode* next = page_head(pa)->next;
			struct blockNode* p;
			for (p = &pageChain; p->next != pa; p = p->next);
			p->next = next;
			// printf("free\n");
			kfree(pa);
		}
		release(&pagelock);
	}
	// printf("free end\n");
}

int
initpage(void* page_start)
{
	if (page_start == 0) return 0;
	acquire(&pagelock);
	is_initializing = 1;
	// 首节点存放下一个页的指针,当前页内block的freelist和已分配块的数量
	page_head(page_start)->next = pageChain.next;
	pageChain.next = page_head(page_start);
	page_freelist(page_start)->next = 0l;
	*page_allocatedBlocks(page_start) = 0;
	char* p;
	for (p = page_start + PGHEAD_SIZE; p + sizeof(Rbnode) - (char*)page_start <= PGSIZE; p += sizeof(Rbnode))
		blkfree(p);
	is_initializing = 0;
	release(&pagelock);
	return 1;
}

void*
blkalloc()
{
	// printf("alloc start\n");
	struct blockNode* node;
	acquire(&pagelock);
	// 如果page的freelist的next字段为0，则意味着该页已没有空余块，需要继续查找
	void* pa;
	for (pa = pageChain.next; pa != 0 && page_freelist(pa)->next == 0; pa = page_head(pa)->next);

	// 如果pa为0，则说明找不到可分配的块，此时需要申请新的页
	if (pa == 0) {
		// 如果initpage失败，说明当前内存已不够分配更多的页，分配失败
		release(&pagelock);
		if (!initpage(kalloc())) return 0;
		acquire(&pagelock);
		pa = pageChain.next;
	}

	// 分配页内的块
	node = page_freelist(pa)->next;
	page_freelist(pa)->next = node->next;
	// printf("next is %p\n", node->next);
	(*page_allocatedBlocks(pa))++;
	release(&pagelock);
	// printf("alloc end\n");
	return node;
}

void
blkinit()
{
	initlock(&pagelock, "pageChain");
	initlock(&treelock, "treelock");
	if (!initpage(kalloc())) {
		panic("blkinit");
	}
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
	for (Rbnode* pnd = min;; pnd = step(&tree_addr, pnd)) {
		// 如果当前节点为nil，说明没有可分配的块，分配失败
		if (pnd == tree_addr.nil) {
			release(&treelock);
			return 0;
		}
		RbnodeView nd = getView(pnd);
		if (!nd.is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd.size > nbytes) {
			// printf("alloc: %p\n", nd.addr);
			blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, nd)));
			nd.size -= nbytes;
			if (nd.size > 0) {}
			void* addr = blkalloc();
			if (!addr) {
				release(&treelock);
				return 0;
			}
			insert_node(&tree_size, init_node(&tree_size, addr, OFFTOADDR(nd.addr), nd.size, 1, RED));
			find_node(&tree_addr, tree_addr.root, nd)->size -= nbytes;
			// 插入新增的已分配块节点
			void* node_addr_addr = blkalloc();
			if (!node_addr_addr) {
				release(&treelock);
				return 0;
			}
			void* node_addr_size = blkalloc();
			if (!node_addr_size) {
				blkfree(node_addr_addr);
				release(&treelock);
				return 0;
			}
			void* ret = OFFTOADDR(nd.addr) + nd.size;
			Rbnode* newNode_addr = init_node(&tree_addr, node_addr_addr, ret, nbytes, 0, RED);
			Rbnode* newNode_size = init_node(&tree_size, node_addr_size, ret, nbytes, 0, RED);
			insert_node(&tree_addr, newNode_addr);
			insert_node(&tree_size, newNode_size);
			release(&treelock);
			return ret;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd.size == nbytes) {
			nd.is_free = 0;
			find_node(&tree_addr, tree_addr.root, nd)->is_free = 0;
			find_node(&tree_size, tree_size.root, nd)->is_free = 0;
			void* ret = OFFTOADDR(nd.addr);
			release(&treelock);
			return ret;
		}
	}
	// 理论上不执行
	return 0;
}


#elif MODE == 2

RbnodeView last;
// 循环首次适应
void*
khalloc(uint nbytes)
{
	if (nbytes == 0) return 0;
	acquire(&treelock);
	Rbnode* min = getmin(&tree_addr, tree_addr.root);
	if (last.ptr == 0) last = getView(min);
	int flag = 0;
	for (Rbnode* pnd = find_node(&tree_addr, tree_addr.root, last);; pnd = step(&tree_addr, pnd)) {
		if (pnd == tree_addr.nil) {
			pnd = min;
		}
		if (flag && last.addr == pnd->addr) {
			release(&treelock);
			return 0;
		}
		RbnodeView nd = getView(pnd);
		if (!nd.is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd.size > nbytes) {
			blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, nd)));
			nd.size -= nbytes;
			if (nd.size > 0) {}
			void* addr = blkalloc();
			if (!addr) {
				release(&treelock);
				return 0;
			}
			insert_node(&tree_size, init_node(&tree_size, addr, OFFTOADDR(nd.addr), nd.size, 1, RED));
			find_node(&tree_addr, tree_addr.root, nd)->size -= nbytes;
			// 插入新增的已分配块节点
			void* node_addr_addr = blkalloc();
			if (!node_addr_addr) {
				release(&treelock);
				return 0;
			}
			void* node_addr_size = blkalloc();
			if (!node_addr_size) {
				blkfree(node_addr_addr);
				release(&treelock);
				return 0;
			}
			void* ret = OFFTOADDR(nd.addr) + nd.size;
			Rbnode* newNode_addr = init_node(&tree_addr, node_addr_addr, ret, nbytes, 0, RED);
			Rbnode* newNode_size = init_node(&tree_size, node_addr_size, ret, nbytes, 0, RED);
			insert_node(&tree_addr, newNode_addr);
			insert_node(&tree_size, newNode_size);
			release(&treelock);
			last = getView(newNode_addr);
			return ret;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd.size == nbytes) {
			nd.is_free = 0;
			Rbnode* tmp = find_node(&tree_addr, tree_addr.root, nd);
			tmp->is_free = 0;
			find_node(&tree_size, tree_size.root, nd)->is_free = 0;
			void* ret = OFFTOADDR(nd.addr);
			release(&treelock);
			last = getView(tmp);
			return ret;
		}
		flag = 1;
	}
	// 理论上不执行
	return 0;
}

#elif MODE == 3
// 最佳适应
void*
khalloc(uint nbytes)
{
	if (nbytes == 0) return 0;
	acquire(&treelock);
	Rbnode* min = getmin(&tree_size, tree_size.root);
	for (Rbnode* pnd = min;; pnd = step(&tree_size, pnd)) {
		// 如果当前节点为nil，说明没有可分配的块，分配失败
		if (pnd == tree_size.nil) {
			release(&treelock);
			return 0;
		}
		RbnodeView nd = getView(pnd);
		if (!nd.is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd.size > nbytes) {
			// printf("alloc: %p\n", nd.addr);
			blkfree(remove_node(&tree_size, pnd));
			nd.size -= nbytes;
			if (nd.size > 0) {}
			void* addr = blkalloc();
			if (!addr) {
				release(&treelock);
				return 0;
			}
			insert_node(&tree_size, init_node(&tree_size, addr, OFFTOADDR(nd.addr), nd.size, 1, RED));
			find_node(&tree_addr, tree_addr.root, nd)->size -= nbytes;
			// 插入新增的已分配块节点
			void* node_addr_addr = blkalloc();
			if (!node_addr_addr) {
				release(&treelock);
				return 0;
			}
			void* node_addr_size = blkalloc();
			if (!node_addr_size) {
				blkfree(node_addr_addr);
				release(&treelock);
				return 0;
			}
			void* ret = OFFTOADDR(nd.addr) + nd.size;
			Rbnode* newNode_addr = init_node(&tree_addr, node_addr_addr, ret, nbytes, 0, RED);
			Rbnode* newNode_size = init_node(&tree_size, node_addr_size, ret, nbytes, 0, RED);
			insert_node(&tree_addr, newNode_addr);
			insert_node(&tree_size, newNode_size);
			release(&treelock);
			return ret;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd.size == nbytes) {
			nd.is_free = 0;
			find_node(&tree_addr, tree_addr.root, nd)->is_free = 0;
			find_node(&tree_size, tree_size.root, nd)->is_free = 0;
			void* ret = OFFTOADDR(nd.addr);
			release(&treelock);
			return ret;
		}
	}
	// 理论上不执行
	return 0;
}

#elif MODE == 4
// 最坏适应
void*
khalloc(uint nbytes)
{
	if (nbytes == 0) return 0;
	acquire(&treelock);
	Rbnode* max = getmax(&tree_size, tree_size.root);
	for (Rbnode* pnd = max;; pnd = step_back(&tree_size, pnd)) {
		// 如果当前节点为nil，说明没有可分配的块，分配失败
		if (pnd == tree_size.nil) {
			release(&treelock);
			return 0;
		}
		RbnodeView nd = getView(pnd);
		if (!nd.is_free) continue;
		// 如果申请的空间小于当前块大小，则减小块大小，并更新tree_size的结构
		if (nd.size > nbytes) {
			// printf("alloc: %p\n", nd.addr);
			blkfree(remove_node(&tree_size, pnd));
			nd.size -= nbytes;
			if (nd.size > 0) {}
			void* addr = blkalloc();
			if (!addr) {
				release(&treelock);
				return 0;
			}
			insert_node(&tree_size, init_node(&tree_size, addr, OFFTOADDR(nd.addr), nd.size, 1, RED));
			find_node(&tree_addr, tree_addr.root, nd)->size -= nbytes;
			// 插入新增的已分配块节点
			void* node_addr_addr = blkalloc();
			if (!node_addr_addr) {
				release(&treelock);
				return 0;
			}
			void* node_addr_size = blkalloc();
			if (!node_addr_size) {
				blkfree(node_addr_addr);
				release(&treelock);
				return 0;
			}
			void* ret = OFFTOADDR(nd.addr) + nd.size;
			Rbnode* newNode_addr = init_node(&tree_addr, node_addr_addr, ret, nbytes, 0, RED);
			Rbnode* newNode_size = init_node(&tree_size, node_addr_size, ret, nbytes, 0, RED);
			insert_node(&tree_addr, newNode_addr);
			insert_node(&tree_size, newNode_size);
			release(&treelock);
			return ret;
		}
		// 如果申请的空间正好等于当前块大小，则将对应节点is_free设置为0
		if (nd.size == nbytes) {
			nd.is_free = 0;
			find_node(&tree_addr, tree_addr.root, nd)->is_free = 0;
			find_node(&tree_size, tree_size.root, nd)->is_free = 0;
			void* ret = OFFTOADDR(nd.addr);
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
	if (pa == 0) return;
	if ((uint64)pa < HEAPSTART || (uint64)pa >= PHYSTOP) panic("khfree");
	acquire(&treelock);
	RbnodeView rmNode;
	rmNode.addr = (uint64)pa - HEAPSTART;
	Rbnode* prmNode = find_node(&tree_addr, tree_addr.root, rmNode);
	rmNode = getView(prmNode);
	// 保证free的地址一定是分配出去的地址
	if (prmNode == tree_addr.nil || rmNode.is_free) {
		release(&treelock);
		printf("%p\n", pa);
		printf("is free: %d\n", rmNode.is_free);
		printf("nil is %p\n", tree_addr.nil);
		panic("khfree");
	}
	RbnodeView prev = getView(step_back(&tree_addr, prmNode));
	RbnodeView next = getView(step(&tree_addr, prmNode));
	int integrate_next = 0;
	int integrate_prev = 0;
	RbnodeView rmnd_size = getView(find_node(&tree_size, tree_size.root, rmNode));
	RbnodeView pvnd_size = getView(find_node(&tree_size, tree_size.root, prev));
	((Rbnode*)rmNode.ptr)->is_free = 1;
	((Rbnode*)rmnd_size.ptr)->is_free = 1;
	// 后节点能合并
	if (next.ptr != tree_addr.nil && rmNode.addr + rmNode.size == next.addr && next.is_free) {
		blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, rmnd_size)));
		blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, next)));
		rmnd_size.size += next.size;

		blkfree(remove_node(&tree_addr, find_node(&tree_addr, tree_addr.root, next)));
		blkfree(remove_node(&tree_addr, find_node(&tree_addr, tree_addr.root, rmNode)));
		rmNode.size += next.size;

		integrate_next = 1;
	}
	// 前节点能合并
	if (prev.ptr != tree_addr.nil && prev.addr + prev.size == rmNode.addr && prev.is_free) {
		// 防止重复删除
		if (!integrate_next) {
			blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, rmnd_size)));
			blkfree(remove_node(&tree_addr, find_node(&tree_addr, tree_addr.root, rmNode)));
		}

		blkfree(remove_node(&tree_size, find_node(&tree_size, tree_size.root, pvnd_size)));
		pvnd_size.size += rmnd_size.size;

		blkfree(remove_node(&tree_addr, find_node(&tree_addr, tree_addr.root, prev)));
		prev.size += rmNode.size;


		void* addr1 = blkalloc();
		void* addr2 = blkalloc();
		if (!addr1) {
			release(&treelock);
			return;
		}
		if (!addr2) {
			blkfree(addr1);
			release(&treelock);
			return;
		}
		insert_node(&tree_size, init_node(&tree_size, addr1, OFFTOADDR(pvnd_size.addr), pvnd_size.size, 1, RED));
		insert_node(&tree_addr, init_node(&tree_addr, addr2, OFFTOADDR(prev.addr), prev.size, 1, RED));
		integrate_prev = 1;
	}
	if (integrate_next && !integrate_prev) {
		void* addr1 = blkalloc();
		void* addr2 = blkalloc();
		if (!addr1) {
			release(&treelock);
			return;
		}
		if (!addr2) {
			blkfree(addr1);
			release(&treelock);
			return;
		}
		insert_node(&tree_size, init_node(&tree_size, addr1, OFFTOADDR(rmnd_size.addr), rmnd_size.size, 1, RED));
		insert_node(&tree_addr, init_node(&tree_addr, addr2, OFFTOADDR(rmNode.addr), rmNode.size, 1, RED));
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

	// printf("address tree:\n");
	// print_tree(&tree_addr, tree_addr.root);
	// printf("***\n");
	// printf("size tree:\n");
	// print_tree(&tree_size, tree_size.root);
	// printf("***\n");
	// printf("\n\n");
	int code;
	if ((code = check_violation(&tree_addr, tree_addr.root)) < 0) {
		printf("error code: %d\n", code);
		panic("rbtree");
	}
	// printf("\n");
	if ((code = check_violation(&tree_size, tree_size.root)) < 0) {
		printf("error code: %d\n", code);
		panic("rbtree");
	}
}

#endif
