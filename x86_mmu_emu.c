/*
* x86-mmu-emu
* Copyright (C) 2015 Raphael S. Carvalho <raphael.scarv@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

// NOTE: This implementation is limited to the two-level page table structure.

// FIXME: implement a handler to catch virtual page fault exceptions.

/*
 * Linear address format:
 * 31         22  31         22  11            0
 * ---------------------------------------------
 * |   DIR      |     PAGE      |    OFFSET    |
 * ---------------------------------------------
 */

typedef uint32_t bit_field;
#define nullptr NULL

static inline size_t page_size() {
    return 4096;
}

// LOG2(page_size())
static inline size_t page_shift() {
    return 12;
}

static inline size_t page_directory_index(uintptr_t virtual_address)
{
    return virtual_address >> 22;
}

static inline size_t page_table_index(uintptr_t virtual_address)
{
    return (virtual_address >> page_shift()) & 0x3FF;
}

typedef struct {
    bit_field present : 1;
    bit_field rw : 1; // read/write
    bit_field us : 1; // user/supervisor
    bit_field : 2;
    bit_field a : 1;
    bit_field d : 2;
    bit_field : 2;
    bit_field page_frame_address : 20;
} page_entry __attribute__ ((aligned));

static void* allocate_page(void)
{
    void *p = memalign(page_size(), page_size());
    assert(p != nullptr);
    // assert that addr is indeed aligned to 4096 bytes.
    assert(((uintptr_t)p & (page_size()-1)) == 0);
    memset(p, 0, page_size());
    return p;
}

static inline void set_page_entry(page_entry *entry, uintptr_t addr)
{
    // copy the high-order 20 bits into page_frame_address.
    entry->page_frame_address = addr >> page_shift();
    // set the present bit
    entry->present = 1;
}

static void *get_addr_from_entry(page_entry *entry)
{
    return (void *)((size_t)entry->page_frame_address << page_shift());
}

static void insert_page_into_address_space(page_entry *dir,
    uintptr_t virtual_address, uintptr_t physical_address)
{
    page_entry *dir_entry = &dir[page_directory_index(virtual_address)];
    page_entry *table;

    if (!dir_entry->present) {
        void *page_frame = allocate_page();
        set_page_entry(dir_entry, (uintptr_t)page_frame);
        table = page_frame;
    } else {
        table = get_addr_from_entry(dir_entry);
    }
 
    page_entry *table_entry = &table[page_table_index(virtual_address)];
    set_page_entry(table_entry, physical_address);
}

static void* get_page_from_address_space(page_entry *dir,
    uintptr_t virtual_address)
{
    // FIXME: trigger an exception if an entry isn't present.
    page_entry *dir_entry = &dir[page_directory_index(virtual_address)];
    assert(dir_entry->present);
    page_entry *table = get_addr_from_entry(dir_entry);
    page_entry *table_entry = &table[page_table_index(virtual_address)];
    assert(table_entry->present);
    return get_addr_from_entry(table_entry);
}

int main(void)
{
    // basic assertions
    assert(sizeof(page_entry) == 4);
    if (sizeof(void *) != 4) {
        printf("This program is intended to run on a 32-bits machine. " \
            "If using GCC on a x86-64 machine, compile it with -m32.\n");
        return -1;
    }

    // insert new page directory into the virtual control register 3.
    page_entry *cr3 = allocate_page();

    void *page_frame = allocate_page();
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd == -1) {
        perror("open");
        return -1;
    }
    // fill page_frame with random data
    assert(read(fd, page_frame, page_size()) == page_size());
    if (close(fd) == -1) {
        perror("close");
        return -1;
    }

    /// map the following virtual address into the physical one previously allocated.
    uintptr_t virtual_address = 0x0000F000;
    uintptr_t physical_address = (uintptr_t)page_frame;
    insert_page_into_address_space(cr3, virtual_address, physical_address);

    assert(get_page_from_address_space(cr3, virtual_address) == page_frame);

    free(cr3);
    free(page_frame);

    printf("done!\n");

    return 0;
}