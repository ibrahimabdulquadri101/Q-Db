#ifndef PAGE_H
#define PAGE_H
#include <stdint.h>
const int PAGE_SIZE = 4096;
using PageID = uint32_t;
struct Page
{
    uint8_t data[PAGE_SIZE];
    PageID id;
};

#endif // PAGE_H
