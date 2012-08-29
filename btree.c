#include "btree.h"

/*
 *
 * bpt:  B+ Tree Implementation
 * Copyright (C) 2010  Amittai Aviram  http://www.amittai.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Author:  Amittai Aviram
 *   http://www.amittai.com
 *   amittai.aviram@yale.edu or afa13@columbia.edu
 *   Department of Computer Science
 *   Yale University
 *   P. O. Box 208285
 *   New Haven, CT 06520-8285
 * Date:  26 June 2010
 * Last modified: 28 February 2011
 *
 * This implementation demonstrates the B+ tree data structure
 * for educational purposes, includin insertion, deletion, search, and display
 * of the search path, the leaves, or the whole tree.
 *
 * Must be compiled with a C99-compliant C compiler such as the latest GCC.
 *
 * Usage:  bpt [order]
 * where order is an optional argument (integer 3 <= order <= 20)
 * defined as the maximal number of pointers in any node.
 *
 */

// Uncomment the line below if you are compiling on Windows.
// #define WINDOWS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GLOBALS.

/*The order determines the maximum and minimum
 *number of entries (keys and pointers) in any
 *node.  Every node has at most order - 1 keys and
 *at least (roughly speaking) half that number.
 *Every leaf has as many pointers to data as keys,
 *and every internal node has one more pointer
 *to a subtree than the number of keys.
 *This global variable is initialized to the
 *default value.
 */
int order = BTREE_ORDER;

/*The queue is used to print the tree in
 *level order, starting from the root
 *printing each entire rank on a separate
 *line, finishing with the leaves.
 */
node *queue = NULL;

/*The user can toggle on and off the "verbose"
 *property, which causes the pointer addresses
 *to be printed out in hexadecimal notation
 *next to their corresponding keys.
 */
bool verbose_output = false;

// FUNCTION DEFINITIONS.

// OUTPUT AND UTILITIES

/*First message to the user.
 */
void usage_1(void)
{
	printf("B+ Tree of Order %d.\n", order);
	printf("Following Silberschatz, Korth, Sidarshan, Database Concepts, 5th ed.\n\n");
	printf("To build a B+ tree of a different order, start again and enter the order\n");
	printf("as an integer argument:  bpt <order>  ");
	printf("(3 <= order <=20).\n");
	printf("To start with input from a file of newline-delimited integers, \n"
		"start again and enter ");
	printf("the order followed by the filename:\n"
		"bpt <order> <inputfile> .\n");
}


/*Second message to the user.
 */
void usage_2(void)
{
	printf("Enter any of the following commands after the prompt > :\n");
	printf("\ti <k>  -- Insert <k> (an integer) as both key and value).\n");
	printf("\tf <k>  -- Find the value under key <k>.\n");
	printf("\tp <k> -- Print the path from the root to key k and its associated value.\n");
	printf("\td <k>  -- Delete key <k> and its associated value.\n");
	printf("\tx -- Destroy the whole tree.  Start again with an empty tree of the same order.\n");
	printf("\tt -- Print the B+ tree.\n");
	printf("\tl -- Print the keys of the leaves (bottom row of the tree).\n");
	printf("\tv -- Toggle output of pointer addresses (\"verbose\") in tree and leaves.\n");
	printf("\tq -- Quit. (Or use Ctl-D.)\n");
	printf("\t? -- Print this help message.\n");
}


/*Helper function for printing the
 *tree out.  See print_tree.
 */
void enqueue(node *new_node)
{
	node *c;
	if (queue == NULL) {
		queue = new_node;
		queue->next = NULL;
	}
	else {
		c = queue;
		while(c->next != NULL) {
			c = c->next;
		}
		c->next = new_node;
		new_node->next = NULL;
	}
}

/*Helper function for printing the
 *tree out.  See print_tree.
 */
node *dequeue(void)
{
	node *n = queue;
	queue = queue->next;
	n->next = NULL;
	return n;
}


/*Prints the bottom row of keys
 *of the tree (with their respective
 *pointers, if the verbose_output flag is set.
 */
void print_leaves(node *root)
{
	int i;
	node *c = root;
	if (root == NULL) {
		printf("Empty tree.\n");
		return;
	}
	while (!c->is_leaf)
		c = c->pointers[0];
	while (true) {
		for (i = 0; i < c->num_keys; i++) {
			if (verbose_output)
				printf("%lx ", (unsigned long)c->pointers[i]);
			printf("%d ", c->keys[i]);
		}
		if (verbose_output)
			printf("%lx ", (unsigned long)c->pointers[order - 1]);
		if (c->pointers[order - 1] != NULL) {
			printf(" | ");
			c = c->pointers[order - 1];
		}
		else
			break;
	}
	printf("\n");
}


/*Utility function to give the height
 *of the tree, which length in number of edges
 *of the path from the root to any leaf.
 */
int height(node *root)
{
	int h = 0;
	node *c = root;
	while (!c->is_leaf) {
		c = c->pointers[0];
		h++;
	}
	return h;
}


/*Utility function to give the length in edges
 *of the path from any node to the root.
 */
int path_to_root(node *root, node *child)
{
	int length = 0;
	node *c = child;
	while (c != root) {
		c = c->parent;
		length++;
	}
	return length;
}


/*Prints the B+ tree in the command
 *line in level (rank) order, with the
 *keys in each node and the '|' symbol
 *to separate nodes.
 *With the verbose_output flag set.
 *the values of the pointers corresponding
 *to the keys also appear next to their respective
 *keys, in hexadecimal notation.
 */
void print_tree(node *root)
{
	node *n = NULL;
	int i = 0;
	int rank = 0;
	int new_rank = 0;

	if (root == NULL) {
		printf("Empty tree.\n");
		return;
	}
	queue = NULL;
	enqueue(root);
	while (queue != NULL) {
		n = dequeue();
		if (n->parent != NULL && n == n->parent->pointers[0]) {
			new_rank = path_to_root (root, n);
			if (new_rank != rank) {
				rank = new_rank;
				printf("\n");
			}
		}
		if (verbose_output)
			printf("(%lx)", (unsigned long)n);
		for (i = 0; i < n->num_keys; i++) {
			if (verbose_output)
				printf("%lx ", (unsigned long)n->pointers[i]);
			printf("%d ", n->keys[i]);
		}
		if (!n->is_leaf)
			for (i = 0; i <= n->num_keys; i++)
				enqueue(n->pointers[i]);
		if (verbose_output) {
			if (n->is_leaf)
				printf("%lx ", (unsigned long)n->pointers[order - 1]);
			else
				printf("%lx ", (unsigned long)n->pointers[n->num_keys]);
		}
		printf("| ");
	}
	printf("\n");
}


/*Traces the path from the root to a leaf, searching
 *by key.  Displays information about the path
 *if the verbose flag is set.
 *Returns the leaf containing the given key.
 */
node *find_leaf(node *root, uint16_t key, bool verbose)
{
	int i = 0;
	node *c = root;
	if (c == NULL) {
		if (verbose)
			printf("Empty tree.\n");
		return c;
	}
	while (!c->is_leaf) {
		if (verbose) {
			printf("[");
			for (i = 0; i < c->num_keys - 1; i++)
				printf("%d ", c->keys[i]);
			printf("%d] ", c->keys[i]);
		}
		i = 0;
		while (i < c->num_keys) {
			if (key >= c->keys[i]) i++;
			else break;
		}
		if (verbose)
			printf("%d ->\n", i);
		c = (node *)c->pointers[i];
	}
	if (verbose) {
		printf("Leaf [");
		for (i = 0; i < c->num_keys - 1; i++)
			printf("%d ", c->keys[i]);
		printf("%d] ->\n", c->keys[i]);
	}
	return c;
}

/*Finds and returns the record to which
 *a key refers.
 */
void *find(node *root, uint16_t key, bool verbose)
{
	int i = 0;
	node *c = find_leaf (root, key, verbose);
	if (c == NULL) return NULL;
	for (i = 0; i < c->num_keys; i++)
		if (c->keys[i] == key) break;
	if (i == c->num_keys)
		return NULL;
	else
		return c->pointers[i];
}

/*Finds the appropriate place to
 *split a node that is too big into two.
 */
int cut(int length)
{
	if (length % 2 == 0)
		return length/2;
	else
		return length/2 + 1;
}

// INSERTION

/*Creates a new general node, which can be adapted
 *to serve as either a leaf or an internal node.
 */
node *make_node(void)
{
	node *new_node;
	new_node = malloc(sizeof(node));
	memset(new_node, 0, sizeof(node));
	if (new_node == NULL) {
		perror("Node creation.");
		exit(EXIT_FAILURE);
	}
	new_node->keys = malloc ((order - 1) * sizeof(uint16_t));
	if (new_node->keys == NULL) {
		perror("New node keys array.");
		exit(EXIT_FAILURE);
	}
	new_node->pointers = malloc (order * sizeof(void *));
	if (new_node->pointers == NULL) {
		perror("New node pointers array.");
		exit(EXIT_FAILURE);
	}
	new_node->is_leaf = false;
	new_node->num_keys = 0;
	new_node->parent = NULL;
	new_node->next = NULL;
	return new_node;
}

/*Creates a new leaf by creating a node
 *and then adapting it appropriately.
 */
node *make_leaf(void)
{
	node *leaf = make_node();
	leaf->is_leaf = true;
	return leaf;
}


/*Helper function used in insert_into_parent
 *to find the index of the parent's pointer to
 *the node to the left of the key to be inserted.
 */
int get_left_index(node *parent, node *left)
{
	int left_index = 0;
	while (left_index <= parent->num_keys &&
		parent->pointers[left_index] != left)
		left_index++;
	return left_index;
}

/*Inserts a new pointer to a record and its corresponding
 *key into a leaf.
 *Returns the altered leaf.
 */
node *insert_into_leaf(node *leaf, uint16_t key, struct dir_record *pointer)
{
	int i, insertion_point;

	insertion_point = 0;
	while (insertion_point < leaf->num_keys && leaf->keys[insertion_point] < key)
		insertion_point++;

	for (i = leaf->num_keys; i > insertion_point; i--) {
		leaf->keys[i] = leaf->keys[i - 1];
		leaf->pointers[i] = leaf->pointers[i - 1];
	}
	leaf->keys[insertion_point] = key;
	leaf->pointers[insertion_point] = pointer;
	leaf->num_keys++;
	return leaf;
}


/*Inserts a new key and pointer
 *to a new record into a leaf so as to exceed
 *the tree's order, causing the leaf to be split
 *in half.
 */
node *insert_into_leaf_after_splitting(node *root, node *leaf,
				uint16_t key, struct dir_record *pointer)
{
	node *new_leaf;
	int *temp_keys;
	void **temp_pointers;
	int insertion_index, split, new_key, i, j;

	new_leaf = make_leaf();

	temp_keys = malloc (order * sizeof(uint16_t));
	if (temp_keys == NULL) {
		perror("Temporary keys array.");
		exit(EXIT_FAILURE);
	}

	temp_pointers = malloc (order *sizeof(void *));
	if (temp_pointers == NULL) {
		perror("Temporary pointers array.");
		exit(EXIT_FAILURE);
	}

	insertion_index = 0;
	while (insertion_index < order - 1 && leaf->keys[insertion_index] < key)
		insertion_index++;

	for (i = 0, j = 0; i < leaf->num_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_keys[j] = leaf->keys[i];
		temp_pointers[j] = leaf->pointers[i];
	}

	temp_keys[insertion_index] = key;
	temp_pointers[insertion_index] = pointer;

	leaf->num_keys = 0;

	split = cut(order - 1);

	for (i = 0; i < split; i++) {
		leaf->pointers[i] = temp_pointers[i];
		leaf->keys[i] = temp_keys[i];
		leaf->num_keys++;
	}

	for (i = split, j = 0; i < order; i++, j++) {
		new_leaf->pointers[j] = temp_pointers[i];
		new_leaf->keys[j] = temp_keys[i];
		new_leaf->num_keys++;
	}

	free(temp_pointers);
	free(temp_keys);

	new_leaf->pointers[order - 1] = leaf->pointers[order - 1];
	leaf->pointers[order - 1] = new_leaf;

	for (i = leaf->num_keys; i < order - 1; i++)
		leaf->pointers[i] = NULL;
	for (i = new_leaf->num_keys; i < order - 1; i++)
		new_leaf->pointers[i] = NULL;

	new_leaf->parent = leaf->parent;
	new_key = new_leaf->keys[0];

	return insert_into_parent(root, leaf, new_key, new_leaf);
}


/*Inserts a new key and pointer to a node
 *into a node into which these can fit
 *without violating the B+ tree properties.
 */
node *insert_into_node(node *root, node *n,
			int left_index, uint16_t key, node *right)
{
	int i;

	for (i = n->num_keys; i > left_index; i--) {
		n->pointers[i + 1] = n->pointers[i];
		n->keys[i] = n->keys[i - 1];
	}
	n->pointers[left_index + 1] = right;
	n->keys[left_index] = key;
	n->num_keys++;
	return root;
}


/*Inserts a new key and pointer to a node
 *into a node, causing the node's size to exceed
 *the order, and causing the node to split into two.
 */
node *insert_into_node_after_splitting(node *root, node *old_node, int left_index,
					uint16_t key, node *right)
{
	int i, j, split, k_prime;
	node *new_node, *child;
	int *temp_keys;
	node **temp_pointers;

	/*First create a temporary set of keys and pointers
	 *to hold everything in order, including
	 *the new key and pointer, inserted in their
	 *correct places.
	 *Then create a new node and copy half of the
	 *keys and pointers to the old node and
	 *the other half to the new.
	 */

	temp_pointers = malloc ((order + 1) *sizeof(node *));
	if (temp_pointers == NULL) {
		perror("Temporary pointers array for splitting nodes.");
		exit(EXIT_FAILURE);
	}
	temp_keys = malloc (order *sizeof(uint16_t));
	if (temp_keys == NULL) {
		perror("Temporary keys array for splitting nodes.");
		exit(EXIT_FAILURE);
	}

	for (i = 0, j = 0; i < old_node->num_keys + 1; i++, j++) {
		if (j == left_index + 1) j++;
		temp_pointers[j] = old_node->pointers[i];
	}

	for (i = 0, j = 0; i < old_node->num_keys; i++, j++) {
		if (j == left_index) j++;
		temp_keys[j] = old_node->keys[i];
	}

	temp_pointers[left_index + 1] = right;
	temp_keys[left_index] = key;

	/*Create the new node and copy
	 *half the keys and pointers to the
	 *old and half to the new.
	 */
	split = cut(order);
	new_node = make_node();
	old_node->num_keys = 0;
	for (i = 0; i < split - 1; i++) {
		old_node->pointers[i] = temp_pointers[i];
		old_node->keys[i] = temp_keys[i];
		old_node->num_keys++;
	}
	old_node->pointers[i] = temp_pointers[i];
	k_prime = temp_keys[split - 1];
	for (++i, j = 0; i < order; i++, j++) {
		new_node->pointers[j] = temp_pointers[i];
		new_node->keys[j] = temp_keys[i];
		new_node->num_keys++;
	}
	new_node->pointers[j] = temp_pointers[i];
	free(temp_pointers);
	free(temp_keys);
	new_node->parent = old_node->parent;
	for (i = 0; i <= new_node->num_keys; i++) {
		child = new_node->pointers[i];
		child->parent = new_node;
	}

	/*Insert a new key into the parent of the two
	 *nodes resulting from the split, with
	 *the old node to the left and the new to the right.
	 */

	return insert_into_parent(root, old_node, k_prime, new_node);
}



/*Inserts a new node (leaf or internal node) into the B+ tree.
 *Returns the root of the tree after insertion.
 */
node *insert_into_parent(node *root, node *left, uint16_t key, node *right)
{
	int left_index;
	node *parent;

	parent = left->parent;

	/*Case: new root. */

	if (parent == NULL)
		return insert_into_new_root(left, key, right);

	/*Case: leaf or node. (Remainder of
	 *function body.)
	 */

	/*Find the parent's pointer to the left
	 *node.
	 */

	left_index = get_left_index(parent, left);


	/*Simple case: the new key fits into the node.
	 */

	if (parent->num_keys < order - 1)
		return insert_into_node(root, parent, left_index, key, right);

	/*Harder case:  split a node in order
	 *to preserve the B+ tree properties.
	 */

	return insert_into_node_after_splitting(root, parent, left_index, key, right);
}


/*Creates a new root for two subtrees
 *and inserts the appropriate key into
 *the new root.
 */
node *insert_into_new_root(node *left, uint16_t key, node *right)
{
	node *root = make_node();
	root->keys[0] = key;
	root->pointers[0] = left;
	root->pointers[1] = right;
	root->num_keys++;
	root->parent = NULL;
	left->parent = root;
	right->parent = root;
	return root;
}



/*First insertion:
 *start a new tree.
 */
node *start_new_tree(uint16_t key, struct dir_record *pointer)
{
	node *root = make_leaf();
	root->keys[0] = key;
	root->pointers[0] = pointer;
	root->pointers[order - 1] = NULL;
	root->parent = NULL;
	root->num_keys++;
	return root;
}



/*Master insertion function.
 *Inserts a key and an associated value into
 *the B+ tree, causing the tree to be adjusted
 *however necessary to maintain the B+ tree
 *properties.
 */
node *insert(node *root, uint16_t key, void *value)
{
	struct dir_record *pointer;
	node *leaf;

	/*The current implementation ignores
	 *duplicates.
	 */

	if (find(root, key, false) != NULL)
		return root;

	/*Create a new record for the
	 *value.
	 */
	pointer = value;


	/*Case: the tree does not exist yet.
	 *Start a new tree.
	 */

	if (root == NULL)
		return start_new_tree(key, pointer);


	/*Case: the tree already exists.
	 *(Rest of function body.)
	 */

	leaf = find_leaf(root, key, false);

	/*Case: leaf has room for key and pointer.
	 */

	if (leaf->num_keys < order - 1) {
		leaf = insert_into_leaf(leaf, key, pointer);
		return root;
	}


	/*Case:  leaf must be split.
	 */

	return insert_into_leaf_after_splitting(root, leaf, key, pointer);
}




// DELETION.

/*Utility function for deletion.  Retrieves
 *the index of a node's nearest neighbor (sibling)
 *to the left if one exists.  If not (the node
 *is the leftmost child), returns -1 to signify
 *this special case.
 */
int get_neighbor_index(node *n)
{
	int i;

	/*Return the index of the key to the left
	 *of the pointer in the parent pointing
	 *to n.
	 *If n is the leftmost child, this means
	 *return -1.
	 */
	for (i = 0; i <= n->parent->num_keys; i++)
		if (n->parent->pointers[i] == n)
			return i - 1;

	// Error state.
	printf("Search for nonexistent pointer to node in parent.\n");
	printf("Node:  %#lx\n", (unsigned long)n);
	exit(EXIT_FAILURE);
}


node *remove_entry_from_node(node *n, uint16_t key, node *pointer)
{
	int i, num_pointers;

	// Remove the key and shift other keys accordingly.
	i = 0;
	while (n->keys[i] != key)
		i++;
	for (++i; i < n->num_keys; i++)
		n->keys[i - 1] = n->keys[i];

	// Remove the pointer and shift other pointers accordingly.
	// First determine number of pointers.
	num_pointers = n->is_leaf ? n->num_keys : n->num_keys + 1;
	i = 0;
	while (n->pointers[i] != pointer)
		i++;
	for (++i; i < num_pointers; i++)
		n->pointers[i - 1] = n->pointers[i];


	// One key fewer.
	n->num_keys--;

	// Set the other pointers to NULL for tidiness.
	// A leaf uses the last pointer to point to the next leaf.
	if (n->is_leaf)
		for (i = n->num_keys; i < order - 1; i++)
			n->pointers[i] = NULL;
	else
		for (i = n->num_keys + 1; i < order; i++)
			n->pointers[i] = NULL;

	return n;
}


node *adjust_root(node *root)
{
	node *new_root;

	/*Case: nonempty root.
	 *Key and pointer have already been deleted,
	 *so nothing to be done.
	 */

	if (root->num_keys > 0)
		return root;

	/*Case: empty root.
	 */

	// If it has a child, promote
	// the first (only) child
	// as the new root.

	if (!root->is_leaf) {
		new_root = root->pointers[0];
		new_root->parent = NULL;
	}

	// If it is a leaf (has no children),
	// then the whole tree is empty.

	else
		new_root = NULL;

	free(root->keys);
	free(root->pointers);
	free(root);

	return new_root;
}


/*Coalesces a node that has become
 *too small after deletion
 *with a neighboring node that
 *can accept the additional entries
 *without exceeding the maximum.
 */
node *coalesce_nodes(node *root, node *n, node *neighbor, int neighbor_index, int k_prime)
{
	int i, j, neighbor_insertion_index, n_start, n_end, new_k_prime = 0;
	node *tmp;
	bool split;

	/*Swap neighbor with node if node is on the
	 *extreme left and neighbor is to its right.
	 */

	if (neighbor_index == -1) {
		tmp = n;
		n = neighbor;
		neighbor = tmp;
	}

	/*Starting point in the neighbor for copying
	 *keys and pointers from n.
	 *Recall that n and neighbor have swapped places
	 *in the special case of n being a leftmost child.
	 */

	neighbor_insertion_index = neighbor->num_keys;

	/*
	 *Nonleaf nodes may sometimes need to remain split,
	 *if the insertion of k_prime would cause the resulting
	 *single coalesced node to exceed the limit order - 1.
	 *The variable split is always false for leaf nodes
	 *and only sometimes set to true for nonleaf nodes.
	 */

	split = false;

	/*Case:  nonleaf node.
	 *Append k_prime and the following pointer.
	 *If there is room in the neighbor, append
	 *all pointers and keys from the neighbor.
	 *Otherwise, append only cut(order) - 2 keys and
	 *cut(order) - 1 pointers.
	 */

	if (!n->is_leaf) {

		/*Append k_prime.
		 */

		neighbor->keys[neighbor_insertion_index] = k_prime;
		neighbor->num_keys++;


		/*Case (default):  there is room for all of n's keys and pointers
		 *in the neighbor after appending k_prime.
		 */

		n_end = n->num_keys;

		/*Case (special): k cannot fit with all the other keys and pointers
		 *into one coalesced node.
		 */
		n_start = 0; // Only used in this special case.
		if (n->num_keys + neighbor->num_keys >= order) {
			split = true;
			n_end = cut(order) - 2;
		}

		for (i = neighbor_insertion_index + 1, j = 0; j < n_end; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
			n->num_keys--;
			n_start++;
		}

		/*The number of pointers is always
		 *one more than the number of keys.
		 */

		neighbor->pointers[i] = n->pointers[j];

		/*If the nodes are still split, remove the first key from
		 *n.
		 */
		if (split) {
			new_k_prime = n->keys[n_start];
			for (i = 0, j = n_start + 1; i < n->num_keys; i++, j++) {
				n->keys[i] = n->keys[j];
				n->pointers[i] = n->pointers[j];
			}
			n->pointers[i] = n->pointers[j];
			n->num_keys--;
		}

		/*All children must now point up to the same parent.
		 */

		for (i = 0; i < neighbor->num_keys + 1; i++) {
			tmp = (node *)neighbor->pointers[i];
			tmp->parent = neighbor;
		}
	}

	/*In a leaf, append the keys and pointers of
	 *n to the neighbor.
	 *Set the neighbor's last pointer to point to
	 *what had been n's right neighbor.
	 */

	else {
		for (i = neighbor_insertion_index, j = 0; j < n->num_keys; i++, j++) {
			neighbor->keys[i] = n->keys[j];
			neighbor->pointers[i] = n->pointers[j];
			neighbor->num_keys++;
		}
		neighbor->pointers[order - 1] = n->pointers[order - 1];
	}

	if (!split) {
		root = delete_entry(root, n->parent, k_prime, n);
		free(n->keys);
		free(n->pointers);
		free(n);
	}
	else
		for (i = 0; i < n->parent->num_keys; i++)
			if (n->parent->pointers[i + 1] == n) {
				n->parent->keys[i] = new_k_prime;
				break;
			}

	return root;

}


/*Redistributes entries between two nodes when
 *one has become too small after deletion
 *but its neighbor is too big to append the
 *small node's entries without exceeding the
 *maximum
 */
node *redistribute_nodes(node *root, node *n, node *neighbor, int neighbor_index,
			int k_prime_index, int k_prime)
{
	int i;
	node *tmp;

	/*Case: n has a neighbor to the left.
	 *Pull the neighbor's last key-pointer pair over
	 *from the neighbor's right end to n's left end.
	 */

	if (neighbor_index != -1) {
		if (!n->is_leaf)
			n->pointers[n->num_keys + 1] = n->pointers[n->num_keys];
		for (i = n->num_keys; i > 0; i--) {
			n->keys[i] = n->keys[i - 1];
			n->pointers[i] = n->pointers[i - 1];
		}
		if (!n->is_leaf) {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys];
			tmp = (node *)n->pointers[0];
			tmp->parent = n;
			neighbor->pointers[neighbor->num_keys] = NULL;
			n->keys[0] = k_prime;
			n->parent->keys[k_prime_index] = neighbor->keys[neighbor->num_keys - 1];
		}
		else {
			n->pointers[0] = neighbor->pointers[neighbor->num_keys - 1];
			neighbor->pointers[neighbor->num_keys - 1] = NULL;
			n->keys[0] = neighbor->keys[neighbor->num_keys - 1];
			n->parent->keys[k_prime_index] = n->keys[0];
		}
	}

	/*Case: n is the leftmost child.
	 *Take a key-pointer pair from the neighbor to the right.
	 *Move the neighbor's leftmost key-pointer pair
	 *to n's rightmost position.
	 */

	else {
		if (n->is_leaf) {
			n->keys[n->num_keys] = neighbor->keys[0];
			n->pointers[n->num_keys] = neighbor->pointers[0];
			n->parent->keys[k_prime_index] = neighbor->keys[1];
		}
		else {
			n->keys[n->num_keys] = k_prime;
			n->pointers[n->num_keys + 1] = neighbor->pointers[0];
			tmp = (node *)n->pointers[n->num_keys + 1];
			tmp->parent = n;
			n->parent->keys[k_prime_index] = neighbor->keys[0];
		}
		for (i = 0; i < neighbor->num_keys; i++) {
			neighbor->keys[i] = neighbor->keys[i + 1];
			neighbor->pointers[i] = neighbor->pointers[i + 1];
		}
		if (!n->is_leaf)
			neighbor->pointers[i] = neighbor->pointers[i + 1];
	}

	/*n now has one more key and one more pointer;
	 *the neighbor has one fewer of each.
	 */

	n->num_keys++;
	neighbor->num_keys--;

	return root;
}


/*Deletes an entry from the B+ tree.
 *Removes the record and its key and pointer
 *from the leaf, and then makes all appropriate
 *changes to preserve the B+ tree properties.
 */
node *delete_entry(node *root, node *n, uint16_t key, void *pointer)
{
	int min_keys;
	node *neighbor;
	int neighbor_index;
	int k_prime_index, k_prime;
	int capacity;

	// Remove key and pointer from node.

	n = remove_entry_from_node(n, key, pointer);

	/*Case:  deletion from the root.
	 */

	if (n == root)
		return adjust_root(root);


	/*Case:  deletion from a node below the root.
	 *(Rest of function body.)
	 */

	/*Determine minimum allowable size of node,
	 *to be preserved after deletion.
	 */

	min_keys = n->is_leaf ? cut(order - 1) : cut(order) - 1;

	/*Case:  node stays at or above minimum.
	 *(The simple case.)
	 */

	if (n->num_keys >= min_keys)
		return root;

	/*Case:  node falls below minimum.
	 *Either coalescence or redistribution
	 *is needed.
	 */

	/*Find the appropriate neighbor node with which
	 *to coalesce.
	 *Also find the key (k_prime) in the parent
	 *between the pointer to node n and the pointer
	 *to the neighbor.
	 */

	neighbor_index = get_neighbor_index (n);
	k_prime_index = neighbor_index == -1 ? 0 : neighbor_index;
	k_prime = n->parent->keys[k_prime_index];
	neighbor = neighbor_index == -1 ? n->parent->pointers[1] :
		n->parent->pointers[neighbor_index];

	capacity = n->is_leaf ? order : order - 1;

	/*Coalescence. */

	if (neighbor->num_keys + n->num_keys < capacity)
		return coalesce_nodes(root, n, neighbor, neighbor_index, k_prime);

	/*Redistribution. */

	else
		return redistribute_nodes(root, n, neighbor, neighbor_index, k_prime_index, k_prime);
}

node *delete(node *root, uint16_t key)
{
	node *key_leaf;
	struct dir_record *key_record;

	key_record = find(root, key, false);
	key_leaf = find_leaf(root, key, false);
	if (key_record != NULL && key_leaf != NULL) {
		root = delete_entry(root, key_leaf, key, key_record);
		free(key_record);
	}
	return root;
}

void destroy_tree(node *root)
{
	int i;
	if (root->is_leaf)
		for (i = 0; i < root->num_keys; i++)
			free(root->pointers[i]);
	else
		for (i = 0; i < root->num_keys + 1; i++)
			destroy_tree(root->pointers[i]);
	free(root->pointers);
	free(root->keys);
	free(root);
}
