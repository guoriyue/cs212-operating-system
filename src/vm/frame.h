#define FRAME_TABLE_ERROR __SIZE_MAX__
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "threads/synch.h"
/* Frame table entry */
struct frame_table_entry
{
  uint32_t *frame;
  struct lock frame_lock;
<<<<<<< HEAD
  struct supplementary_page_table_entry *spte;
=======
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
};

struct frame_table
{
  size_t frame_table_entry_number;
  struct frame_table_entry *frame_table_entry;
  uint8_t* base;
<<<<<<< HEAD
  struct lock frame_table_lock;
=======
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
};

struct frame_table *frame_table;

void frame_table_init (size_t frame_table_entry_number, uint8_t* frame_table_base);
size_t frame_table_scan (struct frame_table *f, size_t start, size_t cnt);
bool frame_table_empty (struct frame_table *f, size_t start, size_t cnt);
uint32_t
<<<<<<< HEAD
frame_table_get_id (void* kaddr);
=======
frame_table_get_id (void* kaddr);
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f