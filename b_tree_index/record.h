#pragma once
#include <stdbool.h>

#define INITIAL_VALUE -1

typedef struct Record
{
    long key;
    float a, b, product;
} Record;


// checks if any of record values have initial not correct value
bool is_record_empty(Record record);

// set all values in record to initial
void clear_record(Record* record);

void print_record(Record record);
