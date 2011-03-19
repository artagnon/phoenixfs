#ifndef BTREE_H_
#define BTREE_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

#define BTREE_ORDER 20

/* How many revisions of each file to store */
#define REV_TRUNCATE 20

enum mode_t {
	NODE_REGULAR,
	NODE_SYMLINK,
	NODE_EXECUTABLE,
};

/*Type representing a node in the B+ tree.
 *This type is general enough to serve for both
 *the leaf and the internal node.
 *The heart of the node is the array
 *of keys and the array of corresponding
 *pointers.  The relation between keys
 *and pointers differs between leaves and
 *internal nodes.  In a leaf, the index
 *of each key equals the index of its corresponding
 *pointer, with a maximum of order - 1 key-pointer
 *pairs.  The last pointer points to the
 *leaf to the right (or NULL in the case
 *of the rightmost leaf).
 *In an internal node, the first pointer
 *refers to lower nodes with keys less than
 *the smallest key in the keys array.  Then,
 *with indices i starting at 0, the pointer
 *at i + 1 points to the subtree with keys
 *greater than or equal to the key in this
 *node at index i.
 *The num_keys field is used to keep
 *track of the number of valid keys.
 *In an internal node, the number of valid
 *pointers is always num_keys + 1.
 *In a leaf, the number of valid pointers
 *to data is always num_keys.  The
 *last leaf pointer points to the next leaf.
 */
typedef struct node {
	void **pointers;
	uint16_t *keys;
	struct node *parent;
	bool is_leaf;
	uint16_t num_keys;
	struct node *next; // Used for queue.
} node;

struct file_record {
	unsigned char sha1[20];
	enum mode_t mode;
	size_t size;
	time_t mtime;
};

struct vfile_record {
	unsigned char name[PATH_MAX];
	struct file_record *history[REV_TRUNCATE];
	int8_t HEAD;
};

struct dir_record {
	unsigned char name[PATH_MAX];
	struct node *vroot;
};

// FUNCTION PROTOTYPES.

// Output and utility.

void usage_1(void);
void usage_2(void);
void enqueue(node *new_node);
node *dequeue(void);
int height(node *root);
int path_to_root(node *root, node *child);
void print_leaves(node *root);
void print_tree(node *root);
node *find_leaf(node *root, uint16_t key, bool verbose);
void *find(node *root, uint16_t key, bool verbose);
int cut(int length);

// Insertion.

node *make_node(void);
node *make_leaf(void);
int get_left_index(node *parent, node *left);
node *insert_into_leaf(node *leaf, uint16_t key, struct dir_record *pointer);
node *insert_into_leaf_after_splitting(node *root, node *leaf, uint16_t key, struct dir_record *pointer);
node *insert_into_node(node *root, node *parent,
		int left_index, uint16_t key, node *right);
node *insert_into_node_after_splitting(node *root, node *parent, int left_index,
				uint16_t key, node *right);
node *insert_into_parent(node *root, node *left, uint16_t key, node *right);
node *insert_into_new_root(node *left, uint16_t key, node *right);
node *start_new_tree(uint16_t key, struct dir_record *pointer);
node *insert(node *root, uint16_t key, void *value);

// Deletion.

int get_neighbor_index(node *n);
node *adjust_root(node *root);
node *coalesce_nodes(node *root, node *n, node *neighbor, int neighbor_index, int k_prime);
node *redistribute_nodes(node *root, node *n, node *neighbor, int neighbor_index,
			int k_prime_index, int k_prime);
node *delete_entry(node *root, node *n, uint16_t key, void *pointer);
node *delete(node *root, uint16_t key);
void destroy_tree(node *root);

#endif
