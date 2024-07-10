#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "rbtree.h"

extern char etext[];  // kernel.ld sets this to end of kernel code.
#define L 1 << 0
#define XL 1 << 1
// #define V1

#ifdef V1
void left_rotate(Rbtree* tree, Rbnode* node);
void right_rotate(Rbtree* tree, Rbnode* node);
void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent);
void set_root(Rbtree* tree, Rbnode* node);

void set_left(Rbtree* tree, Rbnode* node, Rbnode* left)
{
	if (node == tree->nil) return;
	node->left = (long)left - BASEADDR;
	set_parent(tree, left, node);
}

void set_right(Rbtree* tree, Rbnode* node, Rbnode* right)
{
	if (node == tree->nil) return;
	node->right = (long)right - BASEADDR;
	set_parent(tree, right, node);
}

void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent)
{
	if (node == tree->nil) return;
	node->parent = (long)parent - BASEADDR;
}

void set_root(Rbtree* tree, Rbnode* node)
{
	tree->root = node;
	set_parent(tree, node, tree->nil);
}

Rbnode* get_left(const Rbnode* const node)
{
	return (Rbnode*)(node->left + BASEADDR);
}

Rbnode* get_right(const Rbnode* const node)
{
	return (Rbnode*)(node->right + BASEADDR);
}

Rbnode* get_parent(const Rbnode* const node)
{
	return (Rbnode*)(node->parent + BASEADDR);
}
void init_rbtree(Rbtree* tree, void* init_addr1, void* init_addr2, int (*comp)(void*, void*))
{
	tree->nil = init_node(tree, init_addr1, 0, 0, 0, BLACK);
	Rbnode* nil = tree->nil;
	nil->left = nil->parent = nil->right = (uint64)init_addr1 - BASEADDR;
	tree->root = init_node(tree, init_addr2, (void*)(PHYSTOP - HEAPLEN), HEAPLEN, 1, BLACK);
	tree->comp = comp;
}

Rbnode* init_node(Rbtree* tree, void* node_addr, void* start_addr, uint64 size, int is_free, Color color)
{
	set_parent(tree, node_addr, tree->nil);
	set_left(tree, node_addr, tree->nil);
	set_right(tree, node_addr, tree->nil);
	((Rbnode*)node_addr)->addr = (uint64)start_addr - (PHYSTOP - HEAPLEN);
	((Rbnode*)node_addr)->size = size;
	((Rbnode*)node_addr)->color = color;
	((Rbnode*)node_addr)->is_free = is_free;
	return (Rbnode*)node_addr;
}
void insert_node(Rbtree* tree, Rbnode* newNode)
{
	set_left(tree, newNode, tree->nil);
	set_right(tree, newNode, tree->nil);
	if (tree->root == tree->nil) {
		set_root(tree, newNode);
		newNode->color = BLACK;
		return;
	}
	newNode->color = RED;
	Rbnode* node = tree->root;
	for (;;) {
		if (tree->comp(node, newNode) <= 0 && get_right(node) == tree->nil) {
			set_right(tree, node, newNode);
			break;
		}
		else if (tree->comp(node, newNode) > 0 && get_left(node) == tree->nil) {
			set_left(tree, node, newNode);
			break;
		}
		else if (tree->comp(node, newNode) <= 0) {
			node = get_right(node);
		}
		else {
			node = get_left(node);
		}
	}
}

void swap_node(Rbtree* tree, Rbnode* node1, Rbnode* node2)
{
	Rbnode tmp = *node1;
	node1->addr = node2->addr;
	node1->is_free = node2->is_free;
	node1->size = node2->size;
	node2->addr = tmp.addr;
	node2->is_free = tmp.is_free;
	node2->size = tmp.size;
}
void* remove_node(Rbtree* tree, Rbnode* rmnode)
{
	Rbnode* node = rmnode;
	// 如果待删除节点没有子节点
	if (get_left(node) == tree->nil && get_right(node) == tree->nil) {
		// 如果是根节点，则将root置空
		Rbnode* parent = get_parent(node);
		if (node == tree->root) {
			tree->root = tree->nil;
			return node;
		}
		if (get_left(parent) == node) set_left(tree, parent, tree->nil);
		else set_right(tree, parent, tree->nil);
		return node;
	}
	// 如果待删除节点有且仅有一个子节点
	Rbnode* left = get_left(node);
	Rbnode* right = get_right(node);
	Rbnode* rightmin = getmin(tree, get_right(node));
	if ((left == tree->nil && right != tree->nil) || (left != tree->nil && right == tree->nil)) {
		if (left != tree->nil) {
			Rbnode* leftmax = getmax(tree, get_left(node));
			swap_node(tree, node, leftmax);
			return remove_node(tree, leftmax);
		}
		else {
			swap_node(tree, node, rightmin);
			return remove_node(tree, rightmin);
		}
	}
	// 如果待删除节点有两个子节点，则交换待删除节点和右子树最小节点的值
	swap_node(tree, node, rightmin);
	return remove_node(tree, rightmin);
}

// 使用comp比较node和objnode，要求node在tree中，但objnode不需要
// 如果未找到，返回nil
Rbnode* find_node(Rbtree* tree, Rbnode* node, RbnodeView objnode_view)
{
	Rbnode objnode;
	objnode.addr = objnode_view.addr;
	objnode.color = objnode_view.color;
	objnode.size = objnode_view.size;
	objnode.is_free = objnode_view.is_free;
	while (1) {
		if (node == tree->nil) return node;
		if (tree->comp(node, &objnode) < 0) {
			node = get_right(node);
			continue;
		}
		if (tree->comp(node, &objnode) > 0) {
			node = get_left(node);
			continue;
		}
		return node;
	}
}
Rbnode* getmin(Rbtree* tree, Rbnode* node)
{
	while (get_left(node) != tree->nil) node = get_left(node);
	return node;
}

Rbnode* getmax(Rbtree* tree, Rbnode* node)
{
	while (get_right(node) != tree->nil) node = get_right(node);
	return node;
}

Rbnode* step(Rbtree* tree, Rbnode* node)
{
	// 如果当前节点存在右子树，则返回右子树的最小节点
	if (get_right(node) != tree->nil) return getmin(tree, get_right(node));

	// 如果不存在右子树，则判断当前节点是父节点的哪个孩子

	Rbnode* parent = get_parent(node);
	// 如果父节点是nil，则当前节点是根节点，此时遍历到最大值，返回nil
	if (parent == tree->nil) return parent;
	// 如果当前节点是父节点的左节点，则返回父节点
	if (get_left(parent) == node) return parent;
	// 如果当前节点是父节点的右节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的下一个节点
	if (get_right(parent) == node)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (get_left(grandparent) == parent) return grandparent;
			parent = grandparent;
		}
	}
	// 理论上不会执行
	return 0;
}

Rbnode* step_back(Rbtree* tree, Rbnode* node)
{
	// 如果当前节点存在左子树，则返回左子树的最大节点
	if (get_left(node) != tree->nil) return getmax(tree, get_left(node));

	// 如果不存在左子树，则判断当前节点是父节点的哪个孩子

	Rbnode* parent = get_parent(node);
	// 如果父节点是nil，则当前节点是根节点，此时遍历到最小值，返回nil
	if (parent == tree->nil) return parent;
	// 如果当前节点是父节点的右节点，则返回父节点
	if (get_right(parent) == node) return parent;
	// 如果当前节点是父节点的左节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的上一个节点
	if (get_left(parent) == node)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (get_right(grandparent) == parent) return grandparent;
			parent = grandparent;
		}
	}
	// 理论上不会执行
	return 0;
}
#else
// 将节点的指针字段定义为目标节点相对data内存段起始地址的偏移量


void left_rotate(Rbtree* tree, Rbnode* node);
void right_rotate(Rbtree* tree, Rbnode* node);
void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent);
void set_root(Rbtree* tree, Rbnode* node);

void set_left(Rbtree* tree, Rbnode* node, Rbnode* left)
{
	if (node == tree->nil) return;
	node->left = (long)left - BASEADDR;
	set_parent(tree, left, node);
}

void set_right(Rbtree* tree, Rbnode* node, Rbnode* right)
{
	if (node == tree->nil) return;
	node->right = (long)right - BASEADDR;
	set_parent(tree, right, node);
}

void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent)
{
	if (node == tree->nil) return;
	node->parent = (long)parent - BASEADDR;
}

void set_root(Rbtree* tree, Rbnode* node)
{
	tree->root = node;
	set_parent(tree, node, tree->nil);
}

Rbnode* get_left(const Rbnode* const node)
{
	return (Rbnode*)(node->left + BASEADDR);
}

Rbnode* get_right(const Rbnode* const node)
{
	return (Rbnode*)(node->right + BASEADDR);
}

Rbnode* get_parent(const Rbnode* const node)
{
	return (Rbnode*)(node->parent + BASEADDR);
}
void init_rbtree(Rbtree* tree, void* init_addr1, void* init_addr2, int (*comp)(void*, void*))
{
	tree->nil = init_node(tree, init_addr1, 0, 0, 0, BLACK);
	Rbnode* nil = tree->nil;
	nil->left = nil->parent = nil->right = (uint64)init_addr1 - BASEADDR;
	tree->root = init_node(tree, init_addr2, (void*)(PHYSTOP - HEAPLEN), HEAPLEN, 1, BLACK);
	tree->comp = comp;
}

Rbnode* init_node(Rbtree* tree, void* node_addr, void* start_addr, uint64 size, int is_free, Color color)
{
	set_parent(tree, node_addr, tree->nil);
	set_left(tree, node_addr, tree->nil);
	set_right(tree, node_addr, tree->nil);
	((Rbnode*)node_addr)->addr = (uint64)start_addr - (PHYSTOP - HEAPLEN);
	((Rbnode*)node_addr)->size = size;
	((Rbnode*)node_addr)->color = color;
	((Rbnode*)node_addr)->is_free = is_free;
	return (Rbnode*)node_addr;
}

void insert_fix(Rbtree* tree, Rbnode* newNode)
{
	// check violation
	// is root
	if (newNode == tree->root) {
		newNode->color = BLACK;
		return;
	}

	int flag = 0;
	Rbnode* parent = get_parent(newNode);
	// if is left
	if (get_left(parent) == newNode) flag |= XL;
	// continuous red node
	if (parent->color == RED) {
		Rbnode* grandparent = get_parent(parent); // parent is not root because it's red
		Rbnode* patcousin;
		if (get_left(grandparent) == parent) flag |= L;
		if (flag & L) patcousin = get_right(grandparent);
		else patcousin = get_left(grandparent);
		// parent and paternal cousin are all red
		if (patcousin->color == RED) {
			patcousin->color = BLACK;
			parent->color = BLACK;
			grandparent->color = RED;
			insert_fix(tree, grandparent);
			return;
		}
		// LL
		if ((flag & L) && (flag & XL)) {
			parent->color = BLACK;
			grandparent->color = RED;
			right_rotate(tree, grandparent);
		}
		// RR
		else if (!(flag & L) && !(flag & XL)) {
			parent->color = BLACK;
			grandparent->color = RED;
			left_rotate(tree, grandparent);
		}
		// LR
		else if ((flag & L) && !(flag & XL)) {
			newNode->color = BLACK;
			grandparent->color = RED;
			left_rotate(tree, parent);
			right_rotate(tree, grandparent);
		}
		// RL
		else {
			newNode->color = BLACK;
			grandparent->color = RED;
			right_rotate(tree, parent);
			left_rotate(tree, grandparent);
		}
	}
}

void insert_node(Rbtree* tree, Rbnode* newNode)
{
	set_left(tree, newNode, tree->nil);
	set_right(tree, newNode, tree->nil);
	if (tree->root == tree->nil) {
		set_root(tree, newNode);
		newNode->color = BLACK;
		return;
	}
	newNode->color = RED;
	Rbnode* node = tree->root;
	for (;;) {
		if (tree->comp(node, newNode) <= 0 && get_right(node) == tree->nil) {
			set_right(tree, node, newNode);
			break;
		}
		else if (tree->comp(node, newNode) > 0 && get_left(node) == tree->nil) {
			set_left(tree, node, newNode);
			break;
		}
		else if (tree->comp(node, newNode) <= 0) {
			node = get_right(node);
		}
		else {
			node = get_left(node);
		}
	}
	insert_fix(tree, newNode);
}

void swap_node(Rbtree* tree, Rbnode* node1, Rbnode* node2)
{
	Rbnode tmp = *node1;
	node1->addr = node2->addr;
	node1->is_free = node2->is_free;
	node1->size = node2->size;
	node2->addr = tmp.addr;
	node2->is_free = tmp.is_free;
	node2->size = tmp.size;
}

void imbalance_fix(Rbtree* tree, Rbnode* node)
{
	if (node == tree->root) return;
	Rbnode* parent = get_parent(node);
	if (get_left(parent) == node)
	{
		Rbnode* brother = get_right(parent);
		if (parent->color == RED)
		{
			if (get_left(brother)->color == BLACK)
			{
				left_rotate(tree, parent);
				return;
			}
			parent->color = BLACK;
			brother->color = RED;
			if (get_left(brother)->color == RED && get_right(brother)->color == BLACK)
			{
				insert_fix(tree, get_left(brother));
				return;
			}
			get_right(brother)->color = BLACK;
			left_rotate(tree, parent);
			return;
		}
		if (brother->color == BLACK)
		{
			if (get_left(brother)->color == BLACK && get_right(brother)->color == BLACK)
			{
				brother->color = RED;
				imbalance_fix(tree, parent);
				return;
			}
			if (get_right(brother)->color == RED)
			{
				get_right(brother)->color = BLACK;
				left_rotate(tree, parent);
				return;
			}
			get_left(brother)->color = BLACK;
			right_rotate(tree, brother);
			left_rotate(tree, parent);
			return;
		}
		parent->color = RED;
		brother->color = BLACK;
		left_rotate(tree, parent);
		imbalance_fix(tree, node);
		return;
	}
	Rbnode* brother = get_left(parent);
	if (parent->color == RED)
	{
		if (get_right(brother)->color == BLACK)
		{
			right_rotate(tree, parent);
			return;
		}
		parent->color = BLACK;
		brother->color = RED;
		if (get_right(brother)->color == RED && get_left(brother)->color == BLACK)
		{
			insert_fix(tree, get_right(brother));
			return;
		}
		get_left(brother)->color = BLACK;
		right_rotate(tree, parent);
		return;
	}
	if (brother->color == BLACK)
	{
		if (get_right(brother)->color == BLACK && get_left(brother)->color == BLACK)
		{
			brother->color = RED;
			imbalance_fix(tree, parent);
			return;
		}
		if (get_left(brother)->color == RED)
		{
			get_left(brother)->color = BLACK;
			right_rotate(tree, parent);
			return;
		}
		get_right(brother)->color = BLACK;
		left_rotate(tree, brother);
		right_rotate(tree, parent);
		return;
	}
	parent->color = RED;
	brother->color = BLACK;
	right_rotate(tree, parent);
	imbalance_fix(tree, node);
	return;
}

// 调用者必须保证传入的tree是合法的红黑树
void remove_fix(Rbtree* tree, Rbnode* node)
{
	Rbnode* parent = get_parent(node);
	Rbnode* brother;
	if (get_left(parent) == node)
	{
		brother = get_right(parent);
		if (parent->color == RED)
		{
			if (get_left(brother) == tree->nil && get_right(brother) == tree->nil)
			{
				parent->color = BLACK;
				brother->color = RED;
				return;
			}
			if (get_left(brother) == tree->nil)
			{
				left_rotate(tree, parent);
				return;
			}
			if (get_right(brother) == tree->nil)
			{
				parent->color = BLACK;
				right_rotate(tree, brother);
				left_rotate(tree, parent);
				return;
			}
			brother->color = RED;
			parent->color = BLACK;
			get_right(brother)->color = BLACK;
			left_rotate(tree, parent);
			return;
		}
		if (brother->color == RED)
		{
			left_rotate(tree, parent);
			left_rotate(tree, parent);
			parent->color = RED;
			brother->color = BLACK;
			if (get_right(parent)->color == RED) insert_fix(tree, get_right(parent));
			return;
		}
		if (get_left(brother) != tree->nil)
		{
			get_left(brother)->color = BLACK;
			right_rotate(tree, brother);
			left_rotate(tree, parent);
			return;
		}
		if (get_right(brother) != tree->nil)
		{
			get_right(brother)->color = BLACK;
			left_rotate(tree, parent);
			return;
		}
		brother->color = RED;
		imbalance_fix(tree, parent);
		return;
	}
	// 有待检查
	brother = get_left(parent);
	if (parent->color == RED)
	{
		if (get_right(brother) == tree->nil && get_left(brother) == tree->nil)
		{
			parent->color = BLACK;
			brother->color = RED;
			return;
		}
		if (get_right(brother) == tree->nil)
		{
			right_rotate(tree, parent);
			return;
		}
		if (get_left(brother) == tree->nil)
		{
			parent->color = BLACK;
			left_rotate(tree, brother);
			right_rotate(tree, parent);
			return;
		}
		brother->color = RED;
		parent->color = BLACK;
		get_left(brother)->color = BLACK;
		right_rotate(tree, parent);
		return;
	}
	if (brother->color == RED)
	{
		right_rotate(tree, parent);
		right_rotate(tree, parent);
		parent->color = RED;
		brother->color = BLACK;
		if (get_left(parent)->color == RED) insert_fix(tree, get_left(parent));
		return;
	}
	if (get_right(brother) != tree->nil)
	{
		get_right(brother)->color = BLACK;
		left_rotate(tree, brother);
		right_rotate(tree, parent);
		return;
	}
	if (get_left(brother) != tree->nil)
	{
		get_left(brother)->color = BLACK;
		right_rotate(tree, parent);
		return;
	}
	brother->color = RED;
	imbalance_fix(tree, parent);
	return;
}

// 调用者需要保证rmnode在tree中
void* remove_node(Rbtree* tree, Rbnode* rmnode)
{
	Rbnode* node = rmnode;
	// 如果待删除节点没有子节点
	if (get_left(node) == tree->nil && get_right(node) == tree->nil) {
		// 如果是根节点，则将root置空
		Rbnode* parent = get_parent(node);
		if (node == tree->root) {
			tree->root = tree->nil;
			// 为了防止在外部改变tree结构，将node的指针字段置零
			node->left = node->right = node->parent = 0;
			return node;
		}
		// 如果是黑色，则需要fix
		if (node->color == BLACK) {
			remove_fix(tree, node);
		}
		if (get_left(parent) == node) set_left(tree, parent, tree->nil);
		else set_right(tree, parent, tree->nil);
		return node;
	}
	// 如果待删除节点有且仅有一个子节点，则其必然是黑色，子节点必然是红色
	Rbnode* left = get_left(node);
	Rbnode* right = get_right(node);
	if ((left == tree->nil && right != tree->nil) || (left != tree->nil && right == tree->nil)) {
		if (left != tree->nil) {
			swap_node(tree, node, left);
			return remove_node(tree, left);
		}
		else {
			swap_node(tree, node, right);
			return remove_node(tree, right);
		}
	}
	// 如果待删除节点有两个子节点，则交换待删除节点和右子树最小节点的值
	Rbnode* rightmin = getmin(tree, get_right(node));
	swap_node(tree, node, rightmin);
	return remove_node(tree, rightmin);
}

// 使用comp比较node和objnode，要求node在tree中，但objnode不需要
// 如果未找到，返回nil
Rbnode* find_node(Rbtree* tree, Rbnode* node, RbnodeView objnode_view)
{
	Rbnode objnode;
	objnode.addr = objnode_view.addr;
	objnode.color = objnode_view.color;
	objnode.size = objnode_view.size;
	objnode.is_free = objnode_view.is_free;
	if (node == tree->nil) return node;
	if (tree->comp(node, &objnode) < 0) return find_node(tree, get_right(node), objnode_view);
	if (tree->comp(node, &objnode) > 0) return find_node(tree, get_left(node), objnode_view);
	return node;
}

void left_rotate(Rbtree* tree, Rbnode* node)
{
	Rbnode* p = get_parent(node);
	Rbnode* r = get_right(node);
	if (r == tree->nil) return;
	Rbnode* rl = get_left(r);

	if (node == tree->root) {
		set_root(tree, r);
	}
	if (get_left(p) == node) set_left(tree, p, r);
	else if (get_right(p) == node) set_right(tree, p, r);
	set_right(tree, node, rl);
	set_left(tree, r, node);
}

void right_rotate(Rbtree* tree, Rbnode* node)
{
	Rbnode* p = get_parent(node);
	Rbnode* l = get_left(node);
	if (l == tree->nil) return;
	Rbnode* lr = get_right(l);

	if (node == tree->root) {
		set_root(tree, l);
	}
	if (get_left(p) == node) set_left(tree, p, l);
	else if (get_right(p) == node) set_right(tree, p, l);
	set_left(tree, node, lr);
	set_right(tree, l, node);
}

Rbnode* getmin(Rbtree* tree, Rbnode* node)
{
	while (get_left(node) != tree->nil) node = get_left(node);
	return node;
}

Rbnode* getmax(Rbtree* tree, Rbnode* node)
{
	while (get_right(node) != tree->nil) node = get_right(node);
	return node;
}

Rbnode* step(Rbtree* tree, Rbnode* node)
{
	// 如果当前节点存在右子树，则返回右子树的最小节点
	if (get_right(node) != tree->nil) return getmin(tree, get_right(node));

	// 如果不存在右子树，则判断当前节点是父节点的哪个孩子

	Rbnode* parent = get_parent(node);
	// 如果父节点是nil，则当前节点是根节点，此时遍历到最大值，返回nil
	if (parent == tree->nil) return parent;
	// 如果当前节点是父节点的左节点，则返回父节点
	if (get_left(parent) == node) return parent;
	// 如果当前节点是父节点的右节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的下一个节点
	if (get_right(parent) == node)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (get_left(grandparent) == parent) return grandparent;
			parent = grandparent;
		}
	}
	// 理论上不会执行
	return 0;
}

Rbnode* step_back(Rbtree* tree, Rbnode* node)
{
	// 如果当前节点存在左子树，则返回左子树的最大节点
	if (get_left(node) != tree->nil) return getmax(tree, get_left(node));

	// 如果不存在左子树，则判断当前节点是父节点的哪个孩子

	Rbnode* parent = get_parent(node);
	// 如果父节点是nil，则当前节点是根节点，此时遍历到最小值，返回nil
	if (parent == tree->nil) return parent;
	// 如果当前节点是父节点的右节点，则返回父节点
	if (get_right(parent) == node) return parent;
	// 如果当前节点是父节点的左节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的上一个节点
	if (get_left(parent) == node)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (get_right(grandparent) == parent) return grandparent;
			parent = grandparent;
		}
	}
	// 理论上不会执行
	return 0;
}
#endif

void print_node(Rbnode* node)
{
	printf("Addr: %p, objAddr: %p, size: 0x%x, is_free: %s, color: %s, left: %p, right: %p, parent: %p\n",
		node,
		node->addr + PHYSTOP - HEAPLEN,
		node->size,
		node->is_free ? "true" : "false",
		node->color == RED ? "RED" : "BLACK",
		get_left(node),
		get_right(node),
		get_parent(node)
	);
}

void print_tree(Rbtree* tree, Rbnode* node)
{
	if (node == tree->nil) return;
	print_tree(tree, get_left(node));
	print_node(node);
	print_tree(tree, get_right(node));
}

RbnodeView getView(Rbnode* node)
{
	RbnodeView view;
	view.addr = node->addr;
	view.color = node->color;
	view.is_free = node->is_free;
	view.ptr = node;
	view.size = node->size;
	return view;
}

int check_violation(Rbtree* tree, Rbnode* node)
{
	if (tree->root->color == RED) {
		printf("root %p is red\n", tree->root);
		return -1;
	}
	if (node == tree->nil) return 1;
	Rbnode* left = get_left(node);
	Rbnode* right = get_right(node);
	if (node->color == RED && (left->color == RED || right->color == RED)) {
		if (left->color == RED)printf("node %p and leftNode %p are red\n", node, left);
		else printf("node %p and rightNode %p are red\n", node, right);
		return -2;
	}
	int left_ht = check_violation(tree, left);
	int right_ht = check_violation(tree, right);
	if (left_ht < 0 || right_ht < 0) return -3;
	if (left_ht != right_ht) {
		printf("current node is %p\n", node);
		printf("left height is %d\n", left_ht);
		printf("right height is %d\n", right_ht);
		return -4;
	}
	return left_ht + (node->color == BLACK ? 1 : 0);
}
