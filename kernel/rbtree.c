#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "rbtree.h"

#define L 1 << 0
#define XL 1 << 1
// #define V1
#ifdef V1
void left_rotate(Rbtree* tree, Rbnode* node);
void right_rotate(Rbtree* tree, Rbnode* node);
void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent);

void set_left(Rbtree* tree, Rbnode* node, Rbnode* left)
{
	if (node == tree->nil) return;
	node->left = (long)left - (long)node;
	set_parent(tree, left, node);
}

void set_right(Rbtree* tree, Rbnode* node, Rbnode* right)
{
	if (node == tree->nil) return;
	node->right = (long)right - (long)node;
	set_parent(tree, right, node);
}

void set_parent(Rbtree* tree, Rbnode* node, Rbnode* parent)
{
	if (node == tree->nil) return;
	node->parent = (long)parent - (long)node;
}

Rbnode* get_left(const Rbnode* const node)
{
	return (Rbnode*)(node->left + (long)node);
}

Rbnode* get_right(const Rbnode* const node)
{
	return (Rbnode*)(node->right + (long)node);
}

Rbnode* get_parent(const Rbnode* const node)
{
	return (Rbnode*)(node->parent + (long)node);
}

void init_rbtree(Rbtree* tree, void* init_addr1, void* init_addr2, int (*comp)(void*, void*))
{
	tree->nil = init_node(tree, init_addr1, 0, 0, 0);
	Rbnode* nil = tree->nil;
	nil->left = nil->parent = nil->right = 0;
	tree->root = init_node(tree, init_addr2, (void*)(PHYSTOP - HEAPLEN), HEAPLEN, 1);
	tree->comp = comp;
	nil->color = BLACK;
	tree->root->color = BLACK;
}

Rbnode* init_node(Rbtree* tree, void* node_addr, void* start_addr, uint64 size, int is_free)
{
	set_parent(tree, node_addr, tree->nil);
	set_left(tree, node_addr, tree->nil);
	set_right(tree, node_addr, tree->nil);
	((Rbnode*)node_addr)->addr = (uint64)start_addr - (PHYSTOP - HEAPLEN);
	((Rbnode*)node_addr)->size = size;
	((Rbnode*)node_addr)->color = RED;
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
	if (newNode->parent + parent->left == 0) flag |= XL;
	// continuous red node
	if (parent->color == RED) {
		Rbnode* grandparent = get_parent(parent); // parent is not root because it's red
		Rbnode* patcousin;
		if (parent->parent + grandparent->left == 0) flag |= L;
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
	if (tree->root == tree->nil) {
		tree->root = newNode;
		return;
	}
	newNode->color = RED;
	set_left(tree, newNode, tree->nil);
	set_right(tree, newNode, tree->nil);

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
	Rbnode* left1 = get_left(node1);
	Rbnode* right1 = get_right(node1);
	Color color1 = node1->color;
	set_left(tree, node1, get_left(node2));
	set_right(tree, node1, get_right(node2));
	set_left(tree, node2, left1);
	set_right(tree, node2, right1);
	node1->color = node2->color;
	node2->color = color1;

	Rbnode* parent1 = get_parent(node1);
	Rbnode* parent2 = get_parent(node2);
	if (node1 == tree->root) {
		tree->root = node2;
		if (parent2->left + node2->parent == 0) set_left(tree, parent2, node1);
		else set_right(tree, parent2, node1);
		set_parent(tree, node2, tree->nil);
	}
	else if (node2 == tree->root) {
		tree->root = node1;
		if (parent1->left + node1->parent == 0) set_left(tree, parent1, node2);
		else set_right(tree, parent1, node2);
		set_parent(tree, node1, tree->nil);
	}
	else {
		int node1_right = parent1->left + node1->parent;
		int node2_right = parent2->left + node2->parent;
		if (node1_right) set_right(tree, parent1, node2);
		else set_left(tree, parent1, node2);
		if (node2_right) set_right(tree, parent2, node1);
		else set_right(tree, parent2, node1);
	}
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
			return node;
		}
		// 如果是红色，则直接删除
		if (node->color == RED) {
			if (parent->left + node->parent == 0) set_left(tree, parent, tree->nil);
			else set_right(tree, parent, tree->nil);
			return node;
		}
		// 否则为黑色
		Rbnode* brother;
		int is_left = 1;
		if (parent->left + node->parent == 0) brother = get_right(parent);
		else {
			brother = get_left(parent);
			is_left = 0;
		}
		Rbnode* brtleft = get_left(brother);
		Rbnode* brtright = get_right(brother);

		// 如果兄弟节点没有子节点
		if (brtleft == tree->nil && brtright == tree->nil) {
			// if (parent->color == RED) {
			parent->color = BLACK;
			brother->color = RED;
			if (is_left) set_left(tree, parent, tree->nil);
			else set_right(tree, parent, tree->nil);
			return node;
			// }

		}
		// 如果兄弟节点有且仅有一个同方向的子节点
		if ((is_left && brtright != tree->nil && brtleft == tree->nil) ||
			(!is_left && brtright == tree->nil && brtleft != tree->nil))
		{
			brother->color = parent->color;
			parent->color = BLACK;
			if (is_left) {
				set_left(tree, parent, tree->nil);
				brtright->color = BLACK;
				left_rotate(tree, parent);
			}
			else {
				set_right(tree, parent, tree->nil);
				brtleft->color = BLACK;
				right_rotate(tree, parent);
			}
			return node;
		}
		// 如果兄弟节点有且仅有一个不同方向的子节点
		if ((is_left && brtright == tree->nil && brtleft != tree->nil) ||
			(!is_left && brtright != tree->nil && brtleft == tree->nil))
		{
			brother->color = RED;
			if (is_left) {
				set_left(tree, parent, tree->nil);
				brtleft->color = BLACK;
				right_rotate(tree, brother);
				left_rotate(tree, parent);
			}
			else {
				set_right(tree, parent, tree->nil);
				brtright->color = BLACK;
				left_rotate(tree, brother);
				right_rotate(tree, parent);
			}
			return node;
		}
		// 如果兄弟节点有两个子节点
		if (brtleft != tree->nil && brtright != tree->nil) {
			// 如果兄弟节点是红色，则将父节点与兄弟节点互换颜色
			if (brother->color == RED) {
				brother->color = BLACK;
				parent->color = RED;
			}
			// 如果兄弟节点是黑色，则将父节点也置为黑色
			else {
				parent->color = BLACK;
			}

			if (is_left) {
				left_rotate(tree, parent);
				set_left(tree, parent, tree->nil);
			}
			else {
				right_rotate(tree, parent);
				set_right(tree, parent, tree->nil);
			}
			return node;
		}
	}
	// 如果待删除节点有且仅有一个子节点，则其必然是黑色，子节点必然是红色
	Rbnode* left = get_left(node);
	Rbnode* right = get_right(node);
	Rbnode* parent = get_parent(node);
	if ((left == tree->nil && right != tree->nil) || (left != tree->nil && right == tree->nil)) {
		// 如果是根节点，那么将子节点作为新的根节点
		if (parent == tree->nil) {
			if (left != tree->nil) {
				tree->root = left;
				left->color = BLACK;
				set_parent(tree, left, tree->nil);
			}
			else {
				tree->root = right;
				right->color = BLACK;
				set_parent(tree, right, tree->nil);
			}
			return node;
		}
		if (node->parent + parent->left == 0) {
			if (left != tree->nil) set_left(tree, parent, left);
			else set_left(tree, parent, right);
		}
		else {
			if (left != tree->nil) set_right(tree, parent, left);
			else set_right(tree, parent, right);
		}
		return node;
	}
	// 如果待删除节点有两个子节点，则交换待删除节点和右子树最小节点的值
	Rbnode* rightmin = getmin(tree, get_right(node));
	swap_node(tree, node, rightmin);
	return remove_node(tree, node);
}

// 使用comp比较node和objnode，要求node在tree中，但objnode不需要
// 如果未找到，返回nil
Rbnode* find_node(Rbtree* tree, Rbnode* node, Rbnode* objnode)
{
	if (node == tree->nil) return node;
	if (tree->comp(node, objnode) < 0) return find_node(tree, get_right(node), objnode);
	if (tree->comp(node, objnode) > 0) return find_node(tree, get_left(node), objnode);
	return node;
}

void left_rotate(Rbtree* tree, Rbnode* node)
{
	Rbnode* p = get_parent(node);
	Rbnode* r = get_right(node);
	if (r == tree->nil) return;
	Rbnode* rl = get_left(r);

	if (node == tree->root) {
		set_parent(tree, r, tree->nil);
		tree->root = r;
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
		set_parent(tree, l, tree->nil);
		tree->root = l;
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
	if (parent->left + node->parent == 0) return parent;
	// 如果当前节点是父节点的右节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的下一个节点
	if (parent->right + node->parent == 0)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (grandparent->left + parent->parent == 0) return grandparent;
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
	if (parent->right + node->parent == 0) return parent;
	// 如果当前节点是父节点的左节点，则以父节点为root的子树已经遍历完成，此时应该返回父节点的上一个节点
	if (parent->left + node->parent == 0)
	{
		Rbnode* grandparent;
		for (;;) {
			grandparent = get_parent(parent);
			if (grandparent == tree->nil) return tree->nil;
			if (grandparent->right + parent->parent == 0) return grandparent;
			parent = grandparent;
		}
	}
	// 理论上不会执行
	return 0;
}
#else
// 将节点的指针字段定义为目标节点相对data内存段起始地址的偏移量
extern char etext[];  // kernel.ld sets this to end of kernel code.

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
	tree->nil = init_node(tree, init_addr1, 0, 0, 0);
	Rbnode* nil = tree->nil;
	nil->left = nil->parent = nil->right = (uint64)init_addr1 - BASEADDR;
	tree->root = init_node(tree, init_addr2, (void*)(PHYSTOP - HEAPLEN), HEAPLEN, 1);
	tree->comp = comp;
	nil->color = BLACK;
	tree->root->color = BLACK;
}

Rbnode* init_node(Rbtree* tree, void* node_addr, void* start_addr, uint64 size, int is_free)
{
	set_parent(tree, node_addr, tree->nil);
	set_left(tree, node_addr, tree->nil);
	set_right(tree, node_addr, tree->nil);
	((Rbnode*)node_addr)->addr = (uint64)start_addr - (PHYSTOP - HEAPLEN);
	((Rbnode*)node_addr)->size = size;
	((Rbnode*)node_addr)->color = RED;
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
// 调用者必须保证传入的tree是合法的红黑树
void remove_fix(Rbtree* tree, Rbnode* node)
{
	if (node == tree->root) {
		node->color = BLACK;
		return;
	}
	Rbnode* parent = get_parent(node);
	Rbnode* brother;
	int is_left = 1;
	if (get_left(parent) == node) brother = get_right(parent);
	else {
		brother = get_left(parent);
		is_left = 0;
	}
	Rbnode* brtleft = get_left(brother);
	Rbnode* brtright = get_right(brother);

	// 如果兄弟节点没有子节点
	if (brtleft == tree->nil && brtright == tree->nil) {
		brother->color = RED;
		if (parent->color == RED) parent->color = BLACK;
		else remove_fix(tree, parent);
		return;
	}
	// 如果兄弟节点有且仅有一个同方向的子节点
	if ((is_left && brtright != tree->nil && brtleft == tree->nil) ||
		(!is_left && brtright == tree->nil && brtleft != tree->nil))
	{
		brother->color = parent->color;
		parent->color = BLACK;
		if (is_left) {
			brtright->color = BLACK;
			left_rotate(tree, parent);
		}
		else {
			brtleft->color = BLACK;
			right_rotate(tree, parent);
		}
		return;
	}
	// 如果兄弟节点有且仅有一个不同方向的子节点
	if ((is_left && brtright == tree->nil && brtleft != tree->nil) ||
		(!is_left && brtright != tree->nil && brtleft == tree->nil))
	{
		brother->color = RED;
		if (is_left) {
			brtleft->color = BLACK;
			right_rotate(tree, brother);
			left_rotate(tree, parent);
		}
		else {
			brtright->color = BLACK;
			left_rotate(tree, brother);
			right_rotate(tree, parent);
		}
		return;
	}
	// 如果兄弟节点有两个子节点
	if (brtleft != tree->nil && brtright != tree->nil) {
		// 如果兄弟节点是红色，则将父节点与兄弟节点互换颜色
		if (brother->color == RED) {
			brother->color = BLACK;
			parent->color = RED;
		}
		// 如果兄弟节点是黑色，则将父节点也置为黑色
		else {
			parent->color = BLACK;
		}

		if (is_left) {
			left_rotate(tree, parent);
		}
		else {
			right_rotate(tree, parent);
		}
		return;
	}
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
