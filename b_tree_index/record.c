#include <stdio.h>
#include "record.h"

// checks if any of record values have initial not correct value
bool is_record_empty(Record record)
{
    if (record.key == INITIAL_VALUE || record.a == INITIAL_VALUE || record.b == INITIAL_VALUE || record.product == INITIAL_VALUE)
        return true;
    return false;
}

// set all values in record to initial
void clear_record(Record* record)
{
    record->key = INITIAL_VALUE;
    record->a = INITIAL_VALUE;
    record->b = INITIAL_VALUE;
    record->product = INITIAL_VALUE;
}

void print_record(Record record)
{
    if (is_record_empty(record))
        printf("EMPTY RECORD\n");
    else
        printf("%ld\t%.2f\t%.2f\t%.2f\n", record.key, record.a, record.b, record.product);
}