#include "defs.h"
#include "types.h"

typedef enum { BLACK, RED } Color;


// left node or right node is nil when left or right is 0
// we use offset to represent linked node.
struct rbnode {
	int parent;
	int left;
	int right;
	unsigned addr;
	unsigned size;
	Color color;
	int is_free;
};

struct rbtree {
	struct rbnode* root;
	struct rbnode* nil;
	int (*comp)(void*, void*);
};

typedef struct rbtree Rbtree;
typedef struct rbnode Rbnode;

void init_rbtree(Rbtree* tree, void* init_addr1, void* init_addr2, int(*comp)(void*, void*));
Rbnode* init_node(Rbtree* tree, void* node_addr, void* start_addr, uint64 size, int is_free);
void insert_node(Rbtree* tree, Rbnode* newNode);
void* remove_node(Rbtree* tree, Rbnode* rmnode);
Rbnode* find_node(Rbtree* tree, Rbnode* node, Rbnode* objnode);
Rbnode* getmin(Rbtree* tree, Rbnode* node);
Rbnode* getmax(Rbtree* tree, Rbnode* node);
Rbnode* step(Rbtree* tree, Rbnode* node);
Rbnode* step_back(Rbtree* tree, Rbnode* node);
void print_tree(Rbtree* tree, Rbnode* node);
