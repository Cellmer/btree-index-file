#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include "disk_operations.h"
#include "main_file_operations.h"
#include "btree.h"
#include "record.h"

// show menu with available options
void show_menu();
void clear_input_buffer();


int main(int argc, char* argv[])
{
    // create empty main file
    FILE* file;
    file = fopen(MAIN_FILE_NAME, "wb");
    if (file == NULL)
    {
        printf("Could not open a file: %s!\n", MAIN_FILE_NAME);
        return;
    }
    fclose(file);

    // initiate record extractor used for insert operation
    RecordExtractor re_for_inserting;
    record_extractor_init(&re_for_inserting, MAIN_FILE_NAME);

    // create root node and initiate index file
    btree_init();

    // execute commands from file
    if (argc > 1)
    {
        process_file_commands(argv[1], &re_for_inserting);
    }


    char option;
    do
    {
        clear_count_get_page();
        clear_count_save_page();
        show_menu();
        scanf("%c", &option);
        switch (option)
        {
        case '1':
            if (!re_for_inserting.opened)
                record_extractor_reopen(&re_for_inserting);
            handle_insert(&re_for_inserting);
            break;
        case '2':
            if (re_for_inserting.opened)
                record_extractor_close(&re_for_inserting);
            handle_find();
            break;
        case '3':
            if (re_for_inserting.opened)
                record_extractor_close(&re_for_inserting);
            handle_update();
            break;
        case '4':
            if (re_for_inserting.opened)
                record_extractor_close(&re_for_inserting);
            handle_delete();
            break;
        case '5':
            if (re_for_inserting.opened)
                record_extractor_close(&re_for_inserting);
            print_sorted_main_file();
            break;
        case '6':
            if (re_for_inserting.opened)
                record_extractor_close(&re_for_inserting);
            print_main_file(MAIN_FILE_NAME);
            break;
        case '7':
            print_index_file();
            break;
        case 'q':
            record_extractor_close(&re_for_inserting);
            break;
        }

        printf("DISK OPERATIONS:\n");
        printf("PAGES SAVED: %d\n", get_count_save_page());
        printf("PAGES LOADED: %d\n\n", get_count_get_page());

    } while (option != 'q');
	return 0;
}


void show_menu()
{
    clear_input_buffer();
    printf("----------------MENU----------------\n");
    printf("Choose one of the following options:\n");
    printf("1) insert a record to the file\n");
    printf("2) find record with given key\n");
    printf("3) update record with given key\n");
    printf("4) delete record with given key\n");
    printf("5) print sorted file\n");
    printf("6) print main file\n");
    printf("7) print index file\n");
    printf("q) quit\n");
}

void clear_input_buffer()
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
        ;
}