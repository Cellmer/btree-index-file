#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "main_file_operations.h"
#include "disk_operations.h"
#include "record.h"
#include "btree.h"

// prints every record of a file
void print_main_file(const char* filename)
{
    printf("%s:\n", filename);
    printf("Key\tP(A)\tP(B)\tP(AB)\n");
    RecordExtractor re;
    record_extractor_init(&re, filename);
    get_block(re.file, &(re.block));
    Record record;
    record = get_next_record(&re);
    while (!re.end_of_file)
    {
        print_record(record);
        record = get_next_record(&re);
    }

    fclose(re.file);
}

void handle_insert(RecordExtractor* re)
{
    Record record;

    do
    {
        printf("Enter key: ");
        scanf("%ld", &record.key);

        printf("Enter P(A): ");
        scanf("%f", &record.a);

        printf("Enter P(B): ");
        scanf("%f", &record.b);

        printf("Enter P(AB): ");
        scanf("%f", &record.product);

        if (record.a < 0.0f || record.a > 1.0f || record.b < 0.0f || record.b > 1.0f || record.product > min(record.a, record.b) || record.product < 0.0f)
        {
            printf("Enter proper values! Try again.\n");
        }
        else
            break;
    } while (1);

    if(btree_insert(record.key, re->record_index == RECORDS_IN_BLOCK ? re->block_offset + sizeof(Block) : re->block_offset) != -1)
        save_record(re, record);
}

void handle_find()
{
    printf("Type key:\n");
    long key;
    scanf("%ld", &key);
    Node node = btree_find_node(key);
    FindOutput fo = btree_find_in_page(node.page, key);
    if (fo.found_address == -1)
    {
        printf("There is no record with that key!\n\n");
        return;
    }
    long block_address = fo.found_address;

    // find record in a file
    Record record;
    FILE* file = fopen(MAIN_FILE_NAME, "rb");
    fseek(file, block_address, SEEK_SET);
    Block block;
    get_block(file, &block);
    fclose(file);

    for (int i = 0; i < RECORDS_IN_BLOCK; i++)
    {
        if (block.records[i].key == key)
            record = block.records[i];
    }
    printf("\nFOUND RECORD:\n");
    print_record(record);
    printf("\n\n");
}

void handle_update()
{
    printf("Type the key of the record you want to update:\n");
    long key;
    scanf("%ld", &key);
    Node node = btree_find_node(key);
    FindOutput fo = btree_find_in_page(node.page, key);
    if (fo.found_address == -1)
    {
        printf("There is no record with that key!\n");
        return;
    }
    long block_address = fo.found_address;

    // find the record in a file
    Record record;
    FILE* file = fopen(MAIN_FILE_NAME, "rb+");
    fseek(file, block_address, SEEK_SET);
    Block block;
    get_block(file, &block);

    int i;
    for (i = 0; i < RECORDS_IN_BLOCK; i++)
    {
        if (block.records[i].key == key)
        {
            record = block.records[i];
            break;
        }
    }

    // print record
    printf("This is the record:\n");
    printf("Key\tP(A)\tP(B)\tP(AB)\n");
    print_record(record);

    // update record
    printf("Type new P(A) value:\n");
    scanf("%f", &(record.a));
    printf("Type new P(B) value:\n");
    scanf("%f", &(record.b));
    printf("Type new P(AB) value:\n");
    scanf("%f", &(record.product));

    // save updated record
    block.records[i] = record;
    fseek(file, block_address, SEEK_SET);
    save_block(file, block);
    fclose(file);
}

void handle_delete()
{
    printf("Type the key of the record you want to delete:\n");
    long key;
    scanf("%ld", &key);
    // find record
    Node node = btree_find_node(key);
    FindOutput fo = btree_find_in_page(node.page, key);
    if (fo.found_address == -1)
    {
        printf("There is no record with that key!\n");
        return;
    }
    long block_address = fo.found_address;


    // delete record from index file
    btree_delete_from_node(node, key);

    // find the record in a file
    Record record;
    FILE* file = fopen(MAIN_FILE_NAME, "rb+");
    fseek(file, block_address, SEEK_SET);
    Block block;
    get_block(file, &block);

    int i;
    for (i = 0; i < RECORDS_IN_BLOCK; i++)
    {
        if (block.records[i].key == key)
        {
            record = block.records[i];
            break;
        }
    }

    // print record
    printf("This is the record you are deleting:\n");
    printf("Key\tP(A)\tP(B)\tP(AB)\n");
    print_record(record);

    // set record as empty
    clear_record(&record);

    // save deleted record
    block.records[i] = record;
    fseek(file, block_address, SEEK_SET);
    save_block(file, block);
    fclose(file);
}

// prints index file in order (firstly node, then his sons and so on)
void print_index_file()
{
    Node node;
    node.page_address = get_root_address();
    get_page(node.page_address, &(node.page));
    btree_print_node(node);
}

// prints all main_file records sorted using btree
void print_sorted_main_file()
{
    // find node with the smallest key;
    Node node = btree_find_node(0);

    // btree is empty
    if (node.page.addresses[0] == -1)
        return;

    Record record = find_record_in_block(node.page.keys[0], node.page.addresses[0]);
    print_record(record);

    int index = 0;
    btree_get_next_record(&node, &index);
    while (index != -1)
    {
        record = find_record_in_block(node.page.keys[index], node.page.addresses[index]);
        print_record(record);
        btree_get_next_record(&node, &index);
    }
}

Record find_record_in_block(long key, long block_offset)
{
    FILE* file = fopen(MAIN_FILE_NAME, "rb");
    fseek(file, block_offset, SEEK_SET);
    Block block;
    get_block(file, &(block));
    fclose(file);

    for (int i = 0; i < RECORDS_IN_BLOCK; i++)
    {
        if (block.records[i].key == key)
            return block.records[i];
    }
}

// execute commands from file
void process_file_commands(const char* filename, RecordExtractor* re)
{
    clear_count_save_page();
    clear_count_get_page();
    FILE* file = fopen(filename, "r");
    char operation;
    Record record;
    while (fscanf(file, "%c ", &operation) == 1)
    {
        switch (operation)
        {
        case 'I':
        {
           /* printf("INSERT:\n");*/
            fscanf(file, "%ld %f %f %f\n", &(record.key), &(record.a), &(record.b), &(record.product));

            if (!re->opened)
                record_extractor_reopen(re);

            if (btree_insert(record.key, re->record_index == RECORDS_IN_BLOCK ? re->block_offset + sizeof(Block) : re->block_offset) != -1)
                save_record(re, record);
            break;
        }

        case 'U':
        {
           /* printf("UPDATE:\n");*/
            fscanf(file, "%ld %f %f %f\n", &(record.key), &(record.a), &(record.b), &(record.product));

            if (re->opened)
                record_extractor_close(re);

            Node node = btree_find_node(record.key);
            FindOutput fo = btree_find_in_page(node.page, record.key);

            long block_address = fo.found_address;

            // find the record in a file
            FILE* main_file = fopen(MAIN_FILE_NAME, "rb+");
            fseek(main_file, block_address, SEEK_SET);
            Block block;
            get_block(main_file, &block);

            int i = 0;
            while (block.records[i].key != record.key)
                i++;

            // save updated record
            block.records[i] = record;
            fseek(main_file, block_address, SEEK_SET);
            save_block(main_file, block);
            fclose(main_file);
            break;
        }

        case 'D':
        {
            /*printf("DELETE:\n");*/
            long key;
            fscanf(file, "%ld\n", &(key));

            if (re->opened)
                record_extractor_close(re);

            // find record
            Node node = btree_find_node(key);
            FindOutput fo = btree_find_in_page(node.page, key);
            if (fo.found_address == -1)
            {
                printf("There is no record with that key!\n");
                break;
            }
            long block_address = fo.found_address;


            // delete record from index file
            btree_delete_from_node(node, key);

            // find the record in a file
            FILE* main_file = fopen(MAIN_FILE_NAME, "rb+");
            fseek(main_file, block_address, SEEK_SET);
            Block block;
            get_block(main_file, &block);

            int i = 0;
            while (block.records[i].key != key)
                i++;

            // set record as empty
            clear_record(&block.records[i]);

            // save deleted record
            fseek(main_file, block_address, SEEK_SET);
            save_block(main_file, block);
            fclose(main_file);
            break;
        }
        }
    }

    printf("DISK OPERATIONS:\n");
    printf("PAGES SAVED: %d\n", get_count_save_page());
    printf("PAGES LOADED: %d\n\n", get_count_get_page());

    fclose(file);
}

