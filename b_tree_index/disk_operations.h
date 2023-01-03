#pragma once
#include <stdbool.h>
#include "record.h"

#define RECORDS_IN_BLOCK 4
#define INDEX_FILE_NAME "index_file.bin"
#define TREE_ORDER 2

// DISK OPERATIONS FOR MAIN FILE

////////// structures //////////////////
typedef struct Block
{
    Record records[RECORDS_IN_BLOCK];
} Block;

// structure that user can use to get and save records
typedef struct RecordExtractor
{
    const char* filename;
    FILE* file;
    Block block;
    int record_index;
    long block_offset;
    bool opened;
    bool end_of_file;
} RecordExtractor;


////////// functions //////////////////
// returns block of data from current position in file (if EOF return -1)
int get_block(FILE* file, Block* block);

// saves block after current position in file
void save_block(FILE* file, Block block);

// set records vaules to default
void clear_block(Block* block);

// initiates record extractor and assignes file to it
void record_extractor_init(RecordExtractor* re, const char* filename);

// flushes current block to file and closes it to prepare it to delete or update oprations
void record_extractor_close(RecordExtractor* re);

// reinitiate structure to the last position in file
void record_extractor_reopen(RecordExtractor* re);

// get next record using block operations
Record get_next_record(RecordExtractor* re);

// save record using block operations
void save_record(RecordExtractor* re, Record record);


// DISK OPERATIONS FOR INDEX FILE

////////// structures //////////////////
//
// structure representing page in index file (one b-tree node is on one page)
typedef struct Page
{
    // if Btree node representing by this page is leaf or not
    bool is_leaf;
    // how many records are currently saved on this page
    long records_saved;
    // keys saved on that page
    long keys[TREE_ORDER * 2];
    // addresses (offsets) of the records associated with given key in the main_file
    long addresses[TREE_ORDER * 2];
    // addresses (offsets) of all child pages of that page
    long sons[TREE_ORDER * 2 + 1];
    long parent;
} Page;

typedef struct Node
{
    Page page;
    long page_address;
} Node;


////////// functions realizing block disk operations //////////////////
// 
// returns page of data from file from the position given by the page_offset
void get_page(long page_offset, Page* page);

// saves page in file at position given in page_offset
void save_page(long page_offset, Page page);

// allocates node, creates empty page and links it with the proper new addres on a disk
void allocate_node(Node* node);

int get_count_save_page();
int get_count_get_page();
void clear_count_save_page();
void clear_count_get_page();
