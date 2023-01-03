#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "disk_operations.h"
#include "record.h"


// returns block of data from current position in file (if EOF return -1)
int get_block(FILE* file, Block* block)
{
    clear_block(block);
    if (fread(block, sizeof(Block), 1, file) != 1)
        return -1;
    return 0;
}

// saves block after current position in file
void save_block(FILE* file, Block block)
{
    fwrite(&block, sizeof(Block), 1, file);
}

// sets records vaules to default
void clear_block(Block* block)
{
    for (int i = 0; i < RECORDS_IN_BLOCK; i++)
    {
        (*block).records[i].key = INITIAL_VALUE;
        (*block).records[i].a = INITIAL_VALUE;
        (*block).records[i].b = INITIAL_VALUE;
        (*block).records[i].product = INITIAL_VALUE;
    }
}

// initiates record extractor and assignes file to it
void record_extractor_init(RecordExtractor* re, const char* filename)
{
    re->filename = filename;
    re->file = fopen(filename, "rb+");
    re->record_index = 0;
    re->block_offset = 0;
    clear_block(&(re->block));
    re->opened = true;
    re->end_of_file = false;
}

// flushes current block to file and closes it to prepare it to delete or update oprations
void record_extractor_close(RecordExtractor* re)
{
    fseek(re->file, re->block_offset, SEEK_SET);
    save_block(re->file, re->block);
    fclose(re->file);
    re->opened = false;
}

// reinitiate structure to the last position in file
void record_extractor_reopen(RecordExtractor* re)
{
    re->file = fopen(re->filename, "rb+");
    fseek(re->file, re->block_offset, SEEK_SET);
    get_block(re->file, &(re->block));
    re->opened = true;
}

// get next record using block operations
Record get_next_record(RecordExtractor* re)
{
    // all of the records of the current block were used
    if (re->record_index == RECORDS_IN_BLOCK)
    {
        re->record_index = 0;
        if (get_block(re->file, &(re->block)) == -1)
            re->end_of_file = true;
    }

    return re->block.records[re->record_index++];
}

// save record using block operations
void save_record(RecordExtractor* re, Record record)
{
    // all of the records of the current block were filled
    if (re->record_index == RECORDS_IN_BLOCK)
    {
        re->record_index = 0;
        fseek(re->file, re->block_offset, SEEK_SET);
        save_block(re->file, re->block);
        clear_block(&(re->block));
        re->block_offset = ftell(re->file);
    }

    re->block.records[re->record_index] = record;
    re->record_index++;
}

int count_save_page = 0;
int count_get_page = 0;

int get_count_save_page()
{
    return count_save_page;
}

int get_count_get_page()
{
    return count_get_page;
}

void clear_count_save_page()
{
    count_save_page = 0;
}

void clear_count_get_page()
{
    count_get_page = 0;
}

// returns page of data from file from the position given by the page_offset
void get_page(long page_offset, Page* page)
{
    count_get_page++;
    FILE* file = fopen(INDEX_FILE_NAME, "rb+");
    if (file == NULL)
    {
        printf("Could not open a file: %s!\n", INDEX_FILE_NAME);
        return;
    }
    fseek(file, page_offset, SEEK_SET);
    fread(page, sizeof(Page), 1, file);
    fclose(file);
}

// saves page in file at position given in page_offset
void save_page(long page_offset, Page page)
{
    count_save_page++;
    FILE* file = fopen(INDEX_FILE_NAME, "rb+");
    if (file == NULL)
    {
        printf("Could not open a file: %s!\n", INDEX_FILE_NAME);
        return;
    }
    fseek(file, page_offset, SEEK_SET);
    fwrite(&page, sizeof(Page), 1, file);
    fclose(file);
}

// variable which determines the address of the next node to allocate
long next_empty_address = 0;

// allocates node, creates empty page and links it with the proper new addres on a disk
void allocate_node(Node* node)
{
    // set every value on a root page to default
    node->page.is_leaf = true;
    node->page.records_saved = 0;
    for (int i = 0; i < TREE_ORDER * 2; i++)
    {
        node->page.addresses[i] = -1;
        node->page.keys[i] = -1;
        node->page.sons[i] = -1;
    }
    node->page.sons[TREE_ORDER * 2] = -1;
    node->page.parent = -1;
    node->page_address = next_empty_address;
    next_empty_address += sizeof(Page);
}
