#pragma once
#include "record.h"
#include "disk_operations.h"
#define MAIN_FILE_NAME "main_file.bin"

// prints every record of a file
void print_main_file(const char* filename);

// prints all main_file records sorted using btree
void print_sorted_main_file();

// prints index file in order (firstly node, then his sons and so on)
void print_index_file();

// insert a record given by the user both in main and index files
void handle_insert(RecordExtractor* re);

// find and print a record with key given by the user
void handle_find();

// update a record given by the user both in main and index files
void handle_update();

// delete a record given by the user both in main and index files
void handle_delete();

// returns record with given key found in block at given address
Record find_record_in_block(long key, long block_offset);

// execute commands from file
void process_file_commands(const char* filename, RecordExtractor* re);