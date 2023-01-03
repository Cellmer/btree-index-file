#pragma once
#include <stdbool.h>
#include "disk_operations.h"

// structure which is output of function btree_find_in_page
typedef struct FindOutput
{
	// if key was found in a node, this is the address ,linked with it
	long found_address;
	// index of a son of current node in which the key might be
	int son_index;
} FindOutput;

// initiate btree, create empty root node and save it to the file
void btree_init();

// insert record with given key and address to the btree
int btree_insert(long key, long address);

// insert record to given node (or try to compensate or split if it is not possible)
int btree_insert_to_node(Node node, long key, long address);

// returns node with given key or ndoe with page address -1 if key was not found
Node btree_find_node(long key);

// searches for key in a single page
FindOutput btree_find_in_page(Page page, long key);

// deletes record with given key from the btree
void btree_delete_from_node(Node node, long key);

// try to compensate node with sibling, if it is not possible return -1
int btree_compensate(Node node, long key, long address);

// try to compensate node with sibling, if it is not possible return -1
int btree_compensate_after_delete(Node node);

// merge node with its brother
void btree_merge(Node node);

// split given node and perform insert operation on its parent
void btree_split(Node node, long key, long address);

long get_root_address();

void btree_print_node(Node node);

// get next record starting from the record of given index in given node, return new record node and index in the same arguments
void btree_get_next_record(Node* node, int* index);

