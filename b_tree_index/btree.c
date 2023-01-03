#define _CRT_SECURE_NO_WARNINGS
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "btree.h"


long root_address;

long get_root_address()
{
	return root_address;
}

// initiate btree, create empty root node and save it to the file
void btree_init()
{
	// create empty index file
	FILE* file;
	file = fopen(INDEX_FILE_NAME, "wb");
	if (file == NULL)
	{
		printf("Could not open a file: %s!\n", INDEX_FILE_NAME);
		return;
	}
	fclose(file);

	Node root;
	allocate_node(&root);
	// save cleared root on a newly created index file
	save_page(root.page_address, root.page);
	root_address = root.page_address;
}

// insert record with given key and address to the btree
int btree_insert(long key, long address)
{
	// find a node candidate to insert key into
	Node node = btree_find_node(key);

	// insert key starting from fiven node
	if (btree_insert_to_node(node, key, address) == -1)
		return -1;
	return 0;
}

// insert record to given node (or try to compensate or split if it is not possible)
int btree_insert_to_node(Node node, long key, long address)
{
	FindOutput addresses = btree_find_in_page(node.page, key);
	if (addresses.found_address != -1)
	{
		printf("Record with this key already exists!\n");
		return -1;
	}

	// page is not full, insert the key to it
	if (node.page.records_saved < 2 * TREE_ORDER)
	{
		// move keys, addresses and sons to the right to make place for the new key
		for (int i = node.page.records_saved - 1; i >= addresses.son_index; i--)
		{
			node.page.keys[i + 1] = node.page.keys[i];
			node.page.addresses[i + 1] = node.page.addresses[i];
			node.page.sons[i + 2] = node.page.sons[i + 1];
		}
		node.page.sons[addresses.son_index + 1] = node.page.sons[addresses.son_index];
		node.page.sons[addresses.son_index] = -1;
		node.page.keys[addresses.son_index] = key;
		node.page.addresses[addresses.son_index] = address;
		node.page.records_saved++;
		save_page(node.page_address, node.page);
		return 0;
	}

	// try to compensate
	if (btree_compensate(node, key, address) != -1)
		return 0;

	btree_split(node, key, address);
	return 0;
}

// returns address of block containing record with given key or the index of son that might contain that key
FindOutput btree_find_in_page(Page page, long key)
{
	int i = 0;
	while (i < page.records_saved && key > page.keys[i])
		i++;

	FindOutput s;
	if (i < page.records_saved && key == page.keys[i])
	{
		s.found_address = page.addresses[i];
		s.son_index = -1;
		
	}
	else
	{
		s.found_address = -1;
		s.son_index = i;
	}
	return s;
}

// returns node with given key or node in which the key should be inserted
Node btree_find_node(long key)
{
	Node node;
	node.page_address = root_address;
	get_page(node.page_address, &(node.page));
	FindOutput addresses = btree_find_in_page(node.page, key);
	// search until key is not found in leaf
	while (addresses.found_address == -1 && !node.page.is_leaf)
	{
		node.page_address = node.page.sons[addresses.son_index];
		get_page(node.page_address, &(node.page));
		addresses = btree_find_in_page(node.page, key);
	}
	return node;
}

// deletes record with given key from the btree
void btree_delete_from_node(Node node, long key)
{
	// find the index of given key in that node
	int key_index = 0;
	while (node.page.keys[key_index] != key)
		key_index++;

	// if node is not a leaf replace the key from the successor key from the leaf node
	if (!node.page.is_leaf)
	{
		Node successor_node;
		successor_node.page_address = node.page.sons[key_index + 1];
		get_page(successor_node.page_address, &(successor_node.page));
		while (!successor_node.page.is_leaf)
		{
			successor_node.page_address = successor_node.page.sons[0];
			get_page(successor_node.page_address, &(successor_node.page));
		}

		node.page.keys[key_index] = successor_node.page.keys[0];
		node.page.addresses[key_index] = successor_node.page.addresses[0];

		save_page(node.page_address, node.page);

		// remove the replacing key from the leaf
		btree_delete_from_node(successor_node, successor_node.page.keys[0]);
	}
	else
	{
		// remove key from leaf
		for (int i = key_index + 1; i < node.page.records_saved; i++)
		{
			node.page.keys[i - 1] = node.page.keys[i];
			node.page.addresses[i - 1] = node.page.addresses[i];
		}
		node.page.keys[node.page.records_saved - 1] = -1;
		node.page.addresses[node.page.records_saved - 1] = -1;

		node.page.records_saved--;

		save_page(node.page_address, node.page);

		// if node is the root or node have correct number of keys, the deletion process is complete
		if (node.page.parent == -1 || node.page.records_saved >= TREE_ORDER)
			return;

		// try to compensate
		if (btree_compensate_after_delete(node) != -1)
			return;

		btree_merge(node);
	}
}

int btree_compensate_after_delete(Node node)
{
	// if node is root
	if (node.page.parent == -1)
		return -1;

	Node parent;
	parent.page_address = node.page.parent;
	get_page(parent.page_address, &(parent.page));

	// find out which son of parent is current node
	int node_index = 0;
	while (parent.page.sons[node_index] != node.page_address)
		node_index++;

	// try to compensate with the left brother
	if (node_index > 0)
	{
		Node left_brother;
		left_brother.page_address = parent.page.sons[node_index - 1];
		get_page(left_brother.page_address, &(left_brother.page));

		// compensation with left brother is possible
		if (left_brother.page.records_saved > TREE_ORDER)
		{
			int number_of_records = node.page.records_saved + left_brother.page.records_saved + 1;
			long* keys_sorted = (long*)malloc(sizeof(long) * number_of_records);
			long* addresses_sorted = (long*)malloc(sizeof(long) * number_of_records);

			int i = 0;
			for (i = 0; i < left_brother.page.records_saved; i++)
			{
				keys_sorted[i] = left_brother.page.keys[i];
				addresses_sorted[i] = left_brother.page.addresses[i];
			}
			keys_sorted[i] = parent.page.keys[node_index - 1];
			addresses_sorted[i] = parent.page.addresses[node_index - 1];
			i++;
			for (int j = 0; j < node.page.records_saved; j++, i++)
			{
				keys_sorted[i] = node.page.keys[j];
				addresses_sorted[i] = node.page.addresses[j];
			}

			// write records equally between two nodes
			int new_records_number_left_brother = number_of_records / 2;
			for (i = 0; i < new_records_number_left_brother; i++)
			{
				left_brother.page.keys[i] = keys_sorted[i];
				left_brother.page.addresses[i] = addresses_sorted[i];
			}

			parent.page.keys[node_index - 1] = keys_sorted[i];
			parent.page.addresses[node_index - 1] = addresses_sorted[i];
			i++;

			int new_records_number_node = number_of_records - new_records_number_left_brother - 1;
			for (int j = 0; j < new_records_number_node; j++, i++)
			{
				node.page.keys[j] = keys_sorted[i];
				node.page.addresses[j] = addresses_sorted[i];
			}

			// set proper sons
			for (int j = new_records_number_node - node.page.records_saved; j >= 0; j--)
			{
				node.page.sons[j + node.page.records_saved] = node.page.sons[j];
			}

			for (int j = 0, i=new_records_number_left_brother + 1; j < new_records_number_node - node.page.records_saved; j++, i++)
			{
				node.page.sons[j] = left_brother.page.sons[i];
				left_brother.page.sons[i] = -1;
			}

			// clear empty records in left brother
			for (int j = new_records_number_left_brother; j < left_brother.page.records_saved; j++)
			{
				left_brother.page.keys[j] = -1;
				left_brother.page.addresses[j] = -1;
			}

			free(addresses_sorted);
			free(keys_sorted);

			// correct saved_records number in both nodes and save them in file
			node.page.records_saved = new_records_number_node;
			left_brother.page.records_saved = new_records_number_left_brother;
			save_page(node.page_address, node.page);
			save_page(left_brother.page_address, left_brother.page);
			save_page(parent.page_address, parent.page);
			return 0;
		}
	}

	// try to compensate with the right brother
	if (node_index < parent.page.records_saved)
	{
		Node right_brother;
		right_brother.page_address = parent.page.sons[node_index + 1];
		get_page(right_brother.page_address, &(right_brother.page));

		// compensation with right brother is possible
		if (right_brother.page.records_saved > TREE_ORDER)
		{
			int number_of_records = node.page.records_saved + right_brother.page.records_saved + 1;
			long* keys_sorted = (long*)malloc(sizeof(long) * number_of_records);
			long* addresses_sorted = (long*)malloc(sizeof(long) * number_of_records);

			int i;
			for (i = 0; i < node.page.records_saved; i++)
			{
				keys_sorted[i] = node.page.keys[i];
				addresses_sorted[i] = node.page.addresses[i];
			}

			keys_sorted[i] = parent.page.keys[node_index];
			addresses_sorted[i] = parent.page.addresses[node_index];
			i++;

			for (int j = 0; j < right_brother.page.records_saved; j++, i++)
			{
				keys_sorted[i] = right_brother.page.keys[j];
				addresses_sorted[i] = right_brother.page.addresses[j];
			}


			// write records equally between two nodes
			int new_records_number_node = number_of_records / 2;
			for (i = 0; i < new_records_number_node; i++)
			{
				node.page.keys[i] = keys_sorted[i];
				node.page.addresses[i] = addresses_sorted[i];
			}

			parent.page.keys[node_index] = keys_sorted[i];
			parent.page.addresses[node_index] = addresses_sorted[i];
			i++;

			int new_records_number_right_brother = number_of_records - new_records_number_node - 1;
			for (int j = 0; j < new_records_number_right_brother; j++, i++)
			{
				right_brother.page.keys[j] = keys_sorted[i];
				right_brother.page.addresses[j] = addresses_sorted[i];
			}

			// set proper sons
			for (int j = node.page.records_saved + 1, i=0; j <= new_records_number_node; j++, i++)
			{
				node.page.sons[j] = right_brother.page.sons[i];
			}

			for (int j = 0; j <= new_records_number_right_brother; j++)
			{
				right_brother.page.sons[j] = right_brother.page.sons[j + (right_brother.page.records_saved - new_records_number_right_brother)];
				right_brother.page.sons[j + (right_brother.page.records_saved - new_records_number_right_brother)] = -1;
			}

			// clear empty records in right brother
			for (int j = new_records_number_right_brother; j < right_brother.page.records_saved; j++)
			{
				right_brother.page.keys[j] = -1;
				right_brother.page.addresses[j] = -1;
			}

			free(addresses_sorted);
			free(keys_sorted);

			// correct saved_records number in both nodes and save them in file
			node.page.records_saved = new_records_number_node;
			right_brother.page.records_saved = new_records_number_right_brother;
			save_page(node.page_address, node.page);
			save_page(right_brother.page_address, right_brother.page);
			save_page(parent.page_address, parent.page);
			return 0;
		}
	}

	return -1;
}

// merge node with its brother
void btree_merge(Node node)
{
	// if the node is root there is no need to merge
	if (node.page.parent == -1)
		return;

	Node parent;
	parent.page_address = node.page.parent;
	get_page(parent.page_address, &(parent.page));

	// find out which son of parent is current node
	int node_index = 0;
	while (parent.page.sons[node_index] != node.page_address)
		node_index++;

	// merge with left brother
	if (node_index > 0)
	{
		Node left_brother;
		left_brother.page_address = parent.page.sons[node_index - 1];
		get_page(left_brother.page_address, &(left_brother.page));

		// write correct, sorted values to node
		int new_records_number = node.page.records_saved + left_brother.page.records_saved + 1;
		int i, j;
		for (i = node.page.records_saved - 1, j = new_records_number - 1; i >= 0; i--, j--)
		{
			node.page.keys[j] = node.page.keys[i];
			node.page.addresses[j] = node.page.addresses[i];
			node.page.sons[j + 1] = node.page.sons[i + 1];
		}
		node.page.sons[j + 1] = node.page.sons[i + 1];
		
		for (i = 0; i < left_brother.page.records_saved; i++)
		{
			node.page.keys[i] = left_brother.page.keys[i];
			node.page.addresses[i] = left_brother.page.addresses[i];
			node.page.sons[i] = left_brother.page.sons[i];
		}
		node.page.sons[i] = left_brother.page.sons[i];
		node.page.keys[i] = parent.page.keys[node_index - 1];
		node.page.addresses[i] = parent.page.addresses[node_index - 1];
		node.page.records_saved = new_records_number;
	}
	// merge with right brother
	else
	{
		Node right_brother;
		right_brother.page_address = parent.page.sons[node_index + 1];
		get_page(right_brother.page_address, &(right_brother.page));

		// write correct, sorted values to node
		int new_records_number = node.page.records_saved + right_brother.page.records_saved + 1;
		node.page.keys[node.page.records_saved] = parent.page.keys[node_index - 1];
		node.page.addresses[node.page.records_saved] = parent.page.addresses[node_index - 1];
		int i, j;
		for (i = node.page.records_saved + 1, j = 0; i < new_records_number; i++, j++)
		{
			node.page.keys[i] = right_brother.page.keys[j];
			node.page.addresses[i] = right_brother.page.addresses[j];
			node.page.sons[i] = right_brother.page.sons[j];
		}
		node.page.sons[i] = right_brother.page.sons[j];
	}

	// delete key from parent
	int i;
	for (i = node_index; i < parent.page.records_saved - 1; i++)
	{
		parent.page.keys[i] = parent.page.keys[i + 1];
		parent.page.addresses[i] = parent.page.addresses[i + 1];
		parent.page.sons[i] = parent.page.sons[i + 1];
	}
	parent.page.sons[i] = parent.page.sons[i + 1];
	parent.page.sons[i + 1] = -1;
	parent.page.keys[i] = -1;
	parent.page.addresses[i] = -1;
	parent.page.records_saved--;

	// the parent is root
	if (parent.page.parent == -1)
	{
		// the last element from root was taken
		if (parent.page.records_saved == 0)
		{
			root_address = node.page_address;
			node.page.parent = -1;
		}
	}
	else if (parent.page.records_saved < TREE_ORDER)
	{
		if (btree_compensate_after_delete(parent) == -1)
			btree_merge(parent);
	}
	save_page(node.page_address, node.page);
	save_page(parent.page_address, parent.page);

}

// try to compensate node with sibling, if it is not possible return -1
int btree_compensate(Node node, long key, long address)
{
	// node is the root or node is not a leaf
	if (node.page.parent == -1 || !node.page.is_leaf)
		return -1;

	Node parent;
	parent.page_address = node.page.parent;
	get_page(parent.page_address, &(parent.page));

	// find out which son of parent is current node
	int node_index = 0;
	while (parent.page.sons[node_index] != node.page_address)
		node_index++;

	// try to compensate with the left brother
	if (node_index > 0)
	{
		Node left_brother;
		left_brother.page_address = parent.page.sons[node_index - 1];
		get_page(left_brother.page_address, &(left_brother.page));

		// compensation with left brother is possible
		if (left_brother.page.records_saved < 2 * TREE_ORDER)
		{
			int number_of_records = node.page.records_saved + left_brother.page.records_saved + 2;
			long* keys_sorted = (long*)malloc(sizeof(long) * number_of_records);
			long* addresses_sorted = (long*)malloc(sizeof(long) * number_of_records);

			bool new_key_inserted = false;
			int i = 0;
			for (i = 0; i < left_brother.page.records_saved; i++)
			{
				keys_sorted[i] = left_brother.page.keys[i];
				addresses_sorted[i] = left_brother.page.addresses[i];
			}
			keys_sorted[i] = parent.page.keys[node_index - 1];
			addresses_sorted[i] = parent.page.addresses[node_index - 1];
			i++;
			for (int j = 0; j < node.page.records_saved; j++, i++)
			{
				if (key < node.page.keys[j] && !new_key_inserted)
				{
					keys_sorted[i] = key;
					addresses_sorted[i] = address;
					j--;
					new_key_inserted = true;
				}
				else
				{
					keys_sorted[i] = node.page.keys[j];
					addresses_sorted[i] = node.page.addresses[j];
				}
			}

			if (!new_key_inserted)
			{
				keys_sorted[i] = key;
				addresses_sorted[i] = address;
			}

			// write records equally between two nodes
			int new_records_number_left_brother = number_of_records / 2;
			i = 0;
			for (i = 0; i < new_records_number_left_brother; i++)
			{
				left_brother.page.keys[i] = keys_sorted[i];
				left_brother.page.addresses[i] = addresses_sorted[i];
			}

			parent.page.keys[node_index - 1] = keys_sorted[i];
			parent.page.addresses[node_index - 1] = addresses_sorted[i];
			i++;

			int new_records_number_node = number_of_records - new_records_number_left_brother - 1;
			for (int j=0; j < new_records_number_node; j++, i++)
			{
				node.page.keys[j] = keys_sorted[i];
				node.page.addresses[j] = addresses_sorted[i];
			}
			
			// clear empty reacord in node
			for (int j = new_records_number_node; j < node.page.records_saved; j++)
			{
				node.page.keys[j] = -1;
				node.page.addresses[j] = -1;
			}

			free(addresses_sorted);
			free(keys_sorted);

			// correct saved_records number in both nodes and save them in file
			node.page.records_saved = new_records_number_node;
			left_brother.page.records_saved = new_records_number_left_brother;
			save_page(node.page_address, node.page);
			save_page(left_brother.page_address, left_brother.page);
			save_page(parent.page_address, parent.page);
			return 0;
		}
	}

	// try to compensate with the right brother
	if (node_index < parent.page.records_saved)
	{
		Node right_brother;
		right_brother.page_address = parent.page.sons[node_index + 1];
		get_page(right_brother.page_address, &(right_brother.page));

		// compensation with right brother is possible
		if (right_brother.page.records_saved < 2 * TREE_ORDER)
		{
			int number_of_records = node.page.records_saved + right_brother.page.records_saved + 2;
			long* keys_sorted = (long*)malloc(sizeof(long) * number_of_records);
			long* addresses_sorted = (long*)malloc(sizeof(long) * number_of_records);

			bool new_key_inserted = false;

			int i = 0;

			for (int j = 0; j < node.page.records_saved; j++, i++)
			{
				if (key < node.page.keys[j] && !new_key_inserted)
				{
					keys_sorted[i] = key;
					addresses_sorted[i] = address;
					j--;
					new_key_inserted = true;
				}
				else
				{
					keys_sorted[i] = node.page.keys[j];
					addresses_sorted[i] = node.page.addresses[j];
				}
			}

			if (!new_key_inserted)
			{
				keys_sorted[i] = key;
				addresses_sorted[i] = address;
				i++;
			}

			keys_sorted[i] = parent.page.keys[node_index];
			addresses_sorted[i] = parent.page.addresses[node_index];
			i++;

			for (int j = 0; j < right_brother.page.records_saved; j++, i++)
			{
				keys_sorted[i] = right_brother.page.keys[j];
				addresses_sorted[i] = right_brother.page.addresses[j];
			}


			// write records equally between two nodes
			int new_records_number_node = number_of_records / 2;
			i = 0;
			for (i = 0; i < new_records_number_node; i++)
			{
				node.page.keys[i] = keys_sorted[i];
				node.page.addresses[i] = addresses_sorted[i];
			}

			parent.page.keys[node_index] = keys_sorted[i];
			parent.page.addresses[node_index] = addresses_sorted[i];
			i++;

			int new_records_number_right_brother = number_of_records - new_records_number_node - 1;
			for (int j = 0; j < new_records_number_right_brother; j++, i++)
			{
				right_brother.page.keys[j] = keys_sorted[i];
				right_brother.page.addresses[j] = addresses_sorted[i];
			}

			// clear empty records in node
			for (int j = new_records_number_node; j < node.page.records_saved; j++)
			{
				node.page.keys[j] = -1;
				node.page.addresses[j] = -1;
			}

			free(addresses_sorted);
			free(keys_sorted);

			// correct saved_records number in both nodes and save them in file
			node.page.records_saved = new_records_number_node;
			right_brother.page.records_saved = new_records_number_right_brother;
			save_page(node.page_address, node.page);
			save_page(right_brother.page_address, right_brother.page);
			save_page(parent.page_address, parent.page);
			return 0;
		}
	}

	return -1;
}

// split given node and perform insert operation on its parent
void btree_split(Node node, long key, long address)
{
	// create new node
	Node new_node;
	allocate_node(&new_node);
	new_node.page.is_leaf = node.page.is_leaf;

	// create arrays with sorted keys and addresses taking the new key into consideration
	long* keys_sorted = (long*)malloc(sizeof(long) * (node.page.records_saved + 1));
	long* addresses_sorted = (long*)malloc(sizeof(long) * (node.page.records_saved + 1));

	bool new_key_inserted = false;
	int i = 0;
	for (int j = 0; j < node.page.records_saved; j++, i++)
	{
		if (key < node.page.keys[j] && !new_key_inserted)
		{
			keys_sorted[i] = key;
			addresses_sorted[i] = address;
			j--;
			new_key_inserted = true;
		}
		else
		{
			keys_sorted[i] = node.page.keys[j];
			addresses_sorted[i] = node.page.addresses[j];
		}
	}
	if (!new_key_inserted)
	{
		keys_sorted[i] = key;
		addresses_sorted[i] = address;
	}


	// distribute the keys equally
	int new_keys_number = node.page.records_saved / 2;
	for (i = 0; i < new_keys_number; i++)
	{
		new_node.page.keys[i] = keys_sorted[i];
		new_node.page.addresses[i] = addresses_sorted[i];
		if (!new_node.page.is_leaf)
		{
			Node son;
			son.page_address = node.page.sons[i];
			get_page(son.page_address, &(son.page));
			son.page.parent = new_node.page_address;
			save_page(son.page_address, son.page);
			new_node.page.sons[i] = son.page_address;
		}
	}
	if (!new_node.page.is_leaf)
	{
		Node son;
		son.page_address = node.page.sons[i];
		get_page(son.page_address, &(son.page));
		son.page.parent = new_node.page_address;
		save_page(son.page_address, son.page);
		new_node.page.sons[i] = son.page_address;
	}

	int new_key_to_insert = keys_sorted[i];
	int new_address_to_insert = addresses_sorted[i];
	i++;
	for (int j = 0; j < new_keys_number; j++, i++)
	{
		node.page.keys[j] = keys_sorted[i];
		node.page.keys[j + new_keys_number] = -1;
		node.page.addresses[j] = addresses_sorted[i];
		node.page.addresses[j + new_keys_number] = -1;
		node.page.sons[j] = node.page.sons[j + new_keys_number];
		node.page.sons[j + new_keys_number] = -1;
	}
	node.page.sons[new_keys_number] = node.page.sons[node.page.records_saved];
	node.page.sons[node.page.records_saved] = -1;

	node.page.records_saved = new_keys_number;
	new_node.page.records_saved = new_keys_number;

	free(addresses_sorted);
	free(keys_sorted);


	// insert middle key to parent
	// if node is root, create a new root
	if (node.page.parent == -1)
	{
		Node new_root;
		allocate_node(&new_root);
		new_root.page.is_leaf = false;
		root_address = new_root.page_address;
		new_root.page.parent = -1;
		new_root.page.sons[0] = new_node.page_address;
		new_root.page.sons[1] = node.page_address;
		new_root.page.keys[0] = new_key_to_insert;
		new_root.page.addresses[0] = new_address_to_insert;
		new_root.page.records_saved = 1;
		node.page.parent = new_root.page_address;
		new_node.page.parent = new_root.page_address;
		save_page(node.page_address, node.page);
		save_page(new_node.page_address, new_node.page);
		save_page(new_root.page_address, new_root.page);
		return;
	}

	// node is not the root
	Node parent;
	parent.page_address = node.page.parent;
	get_page(parent.page_address, &(parent.page));
	int records_in_parent_before_inserting = parent.page.records_saved;
	btree_insert_to_node(parent, new_key_to_insert, new_address_to_insert);

	// get node page once again in case its parent changed
	Page temp;
	get_page(node.page_address, &(temp));

	// parent of the node changed
	if (temp.parent != parent.page_address)
	{
		Node new_parent;
		new_parent.page_address = temp.parent;
		get_page(new_parent.page_address, &(new_parent));
		int son_index = 0;
		while (new_parent.page.sons[son_index] != node.page_address)
			son_index++;
		// it is the last son of new parent so the node must be saved in another node (old parent)
		if (son_index == new_parent.page.records_saved)
		{
			new_node.page.parent = new_parent.page_address;
			new_parent.page.sons[son_index] = new_node.page_address;
			node.page.parent = parent.page_address;
			save_page(new_parent.page_address, new_parent.page);
			save_page(node.page_address, node.page);
			save_page(new_node.page_address, new_node.page);
			return;
		}
		else
		{
			for (int i = new_parent.page.records_saved; i > son_index + 1; i--)
			{
				new_parent.page.sons[i] = new_parent.page.sons[i - 1];
			}
			new_parent.page.sons[son_index + 1] = node.page_address;
			new_parent.page.sons[son_index] = new_node.page_address;

			node.page.parent = new_parent.page_address;
			new_node.page.parent = new_parent.page_address;

			// get the old parent page and correct its first son address
			get_page(parent.page_address, &(parent));
			Node son;
			son.page_address = parent.page.sons[0];
			get_page(son.page_address, &(son.page));
			son.page.parent = parent.page_address;
			save_page(son.page_address, son.page);
			save_page(new_parent.page_address, new_parent.page);
			save_page(node.page_address, node.page);
			save_page(new_node.page_address, new_node.page);
			save_page(parent.page_address, parent.page);
			return;
		}
	}
	// parent of the node hasn't changed
	else
	{
		get_page(parent.page_address, &(parent.page));
		int son_index = 0;
		while (parent.page.sons[son_index] != node.page_address)
			son_index++;
		// parent was full, so it was splitted too
		if (records_in_parent_before_inserting == 2 * TREE_ORDER)
		{
			for (int i = 0; i < son_index - 1; i++)
			{
				parent.page.sons[i] = parent.page.sons[i + 1];
			}
		}
		parent.page.sons[son_index - 1] = new_node.page_address;
		new_node.page.parent = parent.page_address;
		save_page(node.page_address, node.page);
		save_page(new_node.page_address, new_node.page);
		save_page(parent.page_address, parent.page);
		return;
	}
}

void btree_print_node(Node node)
{
	if (node.page.parent == -1)
		printf("ROOT NODE:\n");
	else
		printf("Parent offset: %ld\n", node.page.parent);

	printf("Page offset: %ld\n", node.page_address);
	printf("Is leaf: %s\n", node.page.is_leaf ? "true" : "false");
	printf("Records saved: %d\n", node.page.records_saved);
	printf("Keys and addresses:\n");
	for (int i = 0; i < 2 * TREE_ORDER; i++)
	{
		printf("(%ld, %ld) ", node.page.keys[i], node.page.addresses[i]);
	}

	if (!node.page.is_leaf)
	{
		printf("\nSons offsets:\n");
		for (int i = 0; i < 2 * TREE_ORDER + 1; i++)
		{
			printf("%ld ", node.page.sons[i]);
		}
		printf("\nSons:\n");
		for (int i = 0; i < node.page.records_saved + 1; i++)
		{
			Node son;
			son.page_address = node.page.sons[i];
			get_page(son.page_address, &(son.page));
			printf("\n\n");
			btree_print_node(son);
		}
	}
	printf("\n\n");
}

// get next record starting from the record of given index in given node, return new record node and index in the same arguments
void btree_get_next_record(Node* node, int* index)
{
	if (node->page.is_leaf)
	{
		// if there is a next record in the same node
		if (*index < node->page.records_saved - 1)
		{
			(*index)++;
			return;
		}

		while(node->page.parent != -1)
		{
			Node parent;
			parent.page_address = node->page.parent;
			get_page(parent.page_address, &(parent.page));

			int son_index = 0;
			while (parent.page.sons[son_index] != node->page_address)
				son_index++;

			// return next record from parent
			if (son_index < parent.page.records_saved)
			{
				*node = parent;
				*index = son_index;
				return;
			}
			
			// it was the last son
			*node = parent;
		}

		// it was the last record
		(*index) = -1;
		return;
	}
	// node is not a leaf
	else
	{
		node->page_address = node->page.sons[*(index)+1];
		get_page(node->page_address, &(node->page));
		while (!node->page.is_leaf)
		{
			node->page_address = node->page.sons[0];
			get_page(node->page_address, &(node->page));
		}

		*(index) = 0;
		return;
	}
}