#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
<<<<<<< HEAD
// #include "vm/swap.h"
=======
#include "vm/swap.h"
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      /* Kill user process. */
      sysexit(-1);  

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  // printf ("start page_fault\n");
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

<<<<<<< HEAD
  if (fault_addr == NULL) {
   kill (f);
  }

  if (!is_user_vaddr (fault_addr)) {
   // thread_exit ();
   kill (f);
=======
  if(!is_user_vaddr (fault_addr)) {
   thread_exit ();
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
  }
  // printf ("supplementary_page_table_entry_find page_fault\n");
  // bad read
  if (not_present && user) {
<<<<<<< HEAD
=======
   // printf ("Page working at %p: %s working %s page in %s context.\n",
   //             fault_addr,
   //             not_present ? "not present" : "rights violation",
   //             write ? "writing" : "reading",
   //             user ? "user" : "kernel");
   // kill(f);
   // Locate the page that faulted.
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f

   struct supplementary_page_table_entry* spte = supplementary_page_table_entry_find (fault_addr);

   if (spte)
   {
      if (spte->location == MAP_MEMORY)
      {
         // printf ("load_page_from_map_memory\n");
         lock_acquire (&spte->page_lock);
         load_page_from_map_memory (spte);
         lock_release (&spte->page_lock);
      }
      else if (spte->location == SWAP_BLOCK)
      {
         // printf ("load_page_from_swap_block\n");
         lock_acquire (&spte->page_lock);
         load_page_from_swap_block (spte);
         lock_release (&spte->page_lock);
      }
      else if (spte->location == FILE_SYSTEM)
      {
         // printf ("load_page_from_file_system\n");
         lock_acquire (&spte->page_lock);
         load_page_from_file_system (spte);
         lock_release (&spte->page_lock);
      }
      else
      {
         sysexit (-1);
      }
<<<<<<< HEAD
      // if (!success) {
      //       sysexit (-1);
      //   } else {
      //       return;
      //   }
   }
   else
   {
      /* Grow stack. */
      // printf ("stack_growth out if fault_addr %d f->esp %d PHYS_BASE %d\n", (uint32_t)fault_addr, (uint32_t)f->esp, (uint32_t)PHYS_BASE);
      if ((fault_addr == f->esp - 4 || fault_addr == f->esp - 32
         || fault_addr >= f->esp) // SUB $n, %esp instruction, and then use a MOV ..., m(%esp) instruction to write to a stack location within the allocated space that is m bytes above the current stack pointer.
         && (uint32_t) fault_addr >= (uint32_t) (PHYS_BASE - 8 * 1024 * 1024) // 8 MB stack size
         && (uint32_t) fault_addr <= (uint32_t) PHYS_BASE)
      {
         // printf ("stack_growth in if\n");
         stack_growth (fault_addr);
      }
      else
      {
         sysexit (-1);
      }
   }

   return ;
  }
=======
   }
   else
   {
      printf ("stack_growth\n");
      // printf ("null spte\n");
      /* Grow stack. */
      if ((fault_addr == f->esp - 4 || fault_addr == f->esp - 32)
         && (uint32_t) fault_addr >= (uint32_t) (PHYS_BASE - 8 * 1024 * 1024) // 8 MB stack size
         && (uint32_t) fault_addr <= (uint32_t) PHYS_BASE)
      {
         // printf ("stack_growth\n");
         stack_growth (fault_addr);
      }
      
   }


   return ;
  }
//   else
//   {
//       // printf ("Page fault at %p: %s error %s page in %s context.\n",
//       //          fault_addr,
//       //          not_present ? "not present" : "rights violation",
//       //          write ? "writing" : "reading",
//       //          user ? "user" : "kernel");
//       kill (f);
//       // sysexit(-1);
//   }



  // printf ("done supplementary_page_table_entry_find page_fault\n");
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f

  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
}

void
stack_growth (void *fault_addr)
{
<<<<<<< HEAD
   // printf ("stack_growth start\n");
=======
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
   void* spid = pg_round_down (fault_addr);
   bool writable = true;

   size_t page_read_bytes = 0;
   size_t page_zero_bytes = 0;

   struct file *file = NULL;
   off_t file_ofs = 0;
   struct supplementary_page_table_entry* spte = supplementary_page_table_entry_create (
      spid,
      writable,
      SWAP_BLOCK,
      page_read_bytes,
      page_zero_bytes,
      file,
      file_ofs
   );
<<<<<<< HEAD
   // printf ("stack_growth reach here\n");
=======
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
   if (spte == NULL) {
      sysexit (-1);
      return ;
   }
   supplementary_page_table_entry_insert (spte);

<<<<<<< HEAD
   // frame_handler_palloc(true, spte, false, true);
=======
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
   void* kaddr = palloc_get_page (PAL_USER | PAL_ZERO);
   struct frame_table_entry fte;
   if (kaddr)
   {
      spte->fid = (void*) frame_table_get_id (kaddr);
      fte = frame_table->frame_table_entry[(uint32_t) spte->fid];
   }
   else
   {
<<<<<<< HEAD
      printf ("eviction need!\n");
      fte.frame = frame_table_evict();
=======
      printf ("eviction\n");
>>>>>>> 45860a9dc820e02aa24dc9bdb7c15b0e081ccb8f
   }
   memset (fte.frame, 0, PGSIZE);

   // struct thread *cur = thread_current ();
   // bool success = false;
   // if (pagedir_get_page (cur->pagedir, spte->pid) == NULL)
   // {
   //    if (pagedir_set_page (cur->pagedir, spte->pid, fte.frame, spte->writable))
   //    {
   //       success = true;
   //    }
   // }

   if (install_page_copy (spte->pid, fte.frame, spte->writable))
   {

   }
   else
   {
      palloc_free_page (kaddr);
      sysexit (-1);
   }
}