#ifndef ROW_H
#define ROW_H
#include <stdint.h>

const int PAGE_SIZE = 4096;

struct Row
{
    int32_t id;
    char name[32];
    int32_t age;
};
const int ROW_SIZE = sizeof(Row);
const int MAX_SLOTS = (PAGE_SIZE - 8) / (ROW_SIZE + 4);
#endif // ROW_H
