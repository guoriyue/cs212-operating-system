#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "devices/timer.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-point.c"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* To calculate load_avg. */
static size_t ready_threads;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
bool flag = true;
static int load_avg;
// defined in fixed-point.c
// static int f = 1 << 14;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);


bool
higher_priority_fun (const struct list_elem *a, const struct list_elem *b, void *aux)
{
  if (list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority)
  {
    return true;
  }
  return false;
}


/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.
   Also initializes the run queue and the tid lock.
   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().
   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  load_avg = 0;

  /* Set priority, recent_cpu and nice values based on scheduler type. */
  int priority = PRI_DEFAULT;
  if (thread_mlfqs){
    initial_thread->nice = 0;
    initial_thread->recent_cpu = 0;
    // initial_thread->priority = PRI_MAX - (initial_thread->recent_cpu / 4) / f - (initial_thread->nice * 2);
    // priority = initial_thread->priority;
  }

  init_thread (initial_thread, "main", priority);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}



void
compute_advanced_parameters (int64_t ticks)
{
  struct thread *t = thread_current ();
  enum intr_level old_level = intr_disable();



  /* At every interrupt we increment the current thread's recent_cpu value by 1. */
  if (thread_mlfqs) {
    if (t != idle_thread) {
      t->recent_cpu = t->recent_cpu + f;
    }
  }
  
  // update load_avg every second
  if (thread_mlfqs && (ticks % TIMER_FREQ) == 0) {
    ready_threads = (t == idle_thread) ? 0 : (list_size(&ready_list) + 1);
    // if (t == idle_thread)
    // {
    //   ready_threads = 0;
    // }
    // else
    // {
    //   ready_threads = 1;
    // }
    // struct list_elem *e;
    // for (e = list_begin (&ready_list); e != list_end (&ready_list);
    //   e = list_next (e))
    // {
    //   struct thread *t = list_entry (e, struct thread, readyelem);
    //   if (t != idle_thread)
    //   {
    //     ready_threads++;
    //   }
    // }
    
    // if (ready_threads == 0 && flag)
    // {
    //   load_avg = 0;
    //   flag = false;
    // } 
    // else
    // {
    
    int tempa = ((59 * f) / 60);
    // printf("load avg before: %d, ready_threads: %d \n", load_avg, ready_threads);
    int tempb = (((int64_t) tempa) * load_avg / f);
    load_avg = tempb + (((1 * f)/60) * ready_threads);

    // int tempa = fixedpoint_mul_fixedpoint(fixedpoint_div_int(int_to_fixedpoint(59), 60), load_avg);
    // int tempb = fixedpoint_mul_int(fixedpoint_div_int(int_to_fixedpoint(1), 60), ready_threads);
    // load_avg = fixedpoint_add_fixedpoint(tempa, tempb);

    // }

    // int tempa = ((59 * f)/60);
    // // printf("load avg before: %d, ready_threads: %d \n", load_avg, ready_threads);
    // int tempb = (((int64_t) tempa) * load_avg / f);
    // load_avg = tempb + (((1 * f)/60) * ready_threads);


    //load_avg = (((int64_t) ((59 * f)/60)) * load_avg / f) + (((1 * f)/60) * ready_threads);
    //printf("%s%d%s\n", "tempa: ", tempa, " should be 1.95");
    //printf("%s%d%s\n", "tempb: ", tempb, " should be 1");
    // printf("%s%d\n", "ready_threads: ", ready_threads);
    //printf("%s%d\n", "load avg after: ", load_avg);

    // update recent_cpu or current thread: recent_cpu = (2*load_avg)/(2*load_avg + 1) * recent_cpu + nice.

    struct list_elem *e;

    for (e = list_begin (&all_list); e != list_end (&all_list);
      e = list_next (e))
    {
      /* once per second the value of recent_cpu is recalculated for every thread */
      struct thread *t1 = list_entry(e, struct thread, allelem);

      if (t1 == idle_thread)
      {
        continue;
      }

      // int temp11 = ((int64_t) (2 * load_avg)) * (f / (2 * load_avg + (f * 1)));
      // int temp21 = ((int64_t) temp11) * ((t1->recent_cpu) / f);
      // t1->recent_cpu = temp21 + (t1->nice * f);
      int temp11 = ((int64_t) (2 * load_avg)) * f;
      int temp111 = (2 * load_avg + (f * 1));
      int temp112 = temp11 / temp111;
      int temp21 = (((int64_t) temp112) * (t1->recent_cpu)) / f;
      t1->recent_cpu = temp21 + (t1->nice * f);
      
    }
  }

  // int priority = PRI_MAX - (t1->recent_cpu / 4) / f - (t1->nice * 2);
  // priority = priority > 63 ? 63 : priority;
  // priority = priority < 0 ? 0 : priority;
  // t1->priority = priority;

  /* Update thread's priority value every fourth clock tick (maybe we can use if statement below). */
  if (thread_mlfqs && (ticks % 4 == 0)) {
    struct list_elem *e;

    for (e = list_begin (&all_list); e != list_end (&all_list);
      e = list_next (e))
    {
      /* once per second the value of recent_cpu is recalculated for every thread */
      struct thread *t1 = list_entry(e, struct thread, allelem);
      if (t1 == idle_thread)
      {
        continue;
      }

      int priority = PRI_MAX - (t1->recent_cpu / 4) / f - (t1->nice * 2);
      priority = priority > 63 ? 63 : priority;
      priority = priority < 0 ? 0 : priority;
      t1->priority = priority;
    }
    /*struct list_elem *front;
    if (!list_empty(&ready_list)) {
      front = list_front(&ready_list);
      struct thread *p = list_entry(front, struct thread, elem);
      if (p->priority > priority) {
        thread_yield();
      }
    }
    */
    list_sort (&ready_list, higher_priority_fun, 0);
  }
  
  intr_set_level(old_level);
}


/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.
   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.
   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Set priority and nice values based on scheduler type. */
  if (thread_mlfqs) {
    t->nice = thread_current()->nice;
    t->recent_cpu = thread_current()->recent_cpu;
    // int priority = PRI_MAX - (t->recent_cpu / 4) / f - (t->nice * 2);
    // priority = priority > 63 ? 63 : priority;
    // priority = priority < 0 ? 0 : priority;
    // t->priority = priority;
  }

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;
  
  /* Add to run queue. */
  thread_unblock (t);
  
  /* Since current already takes cpu, if the new created thread has a higher priority, we should yield current thread. 
     For example, in priority-donate-one, we would get This thread should have priority 32.  Actual priority: 31 if we don't yield. 
     Here the actual priority 31 is the default thread's priority. */
  if (priority > thread_current()->priority)
  {
    thread_yield ();
  }
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().
   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  // printf("start thread_block\n");
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);
  
  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
  // printf("end thread_block\n");
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)
   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  /* Ready list sorted. */
  list_sort (&ready_list, higher_priority_fun, 0);
  
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}
// no interupts when 
// adding non running things in a ready list
// thread current, both are fine
// 
// all thread data before assertion


/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
  {
    /* Keep ready list sorted. */
    list_push_back (&ready_list, &cur->elem);
    if (thread_current() != idle_thread) {
      //printf("%s%s\n", "thread has been added to ready list: ", thread_current()->name);
    }
    list_sort (&ready_list, higher_priority_fun, 0);
  }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  
  enum intr_level old_level = intr_disable ();
  struct thread *cur = thread_current ();
  if(thread_mlfqs)
  {
    return ;
  }
  /* Just simply set the priority. The donation mechanism only changes the donated priorty. */
  int old_priority = cur->priority;
  cur->new_priority = new_priority;

  /* Need a new_priority because we need to keep the original priority for recover. */
  if (!list_empty(&cur->donor_threads))
  {
    /* Have some donor threads, need to make sure that the new priority is greater than the possible donor priority. 
    Otherwise if the new priority is less than the possible donor priority, it breaks the donation rule. */
    struct thread *highest_priority_donor_thread = list_entry (list_begin (&cur->donor_threads), struct thread, donor_thread_elem);

    if ((highest_priority_donor_thread->priority) > new_priority) {
        cur->priority = highest_priority_donor_thread->priority;
    }
  }
  else
  {
    cur->priority = new_priority;
  }

  if(cur->status == THREAD_READY)
  {
    list_sort (&ready_list, higher_priority_fun, 0);
  }

  thread_donate_priority (cur);

  if(old_priority > cur->priority)
  {
    if (!list_empty(&ready_list))
    {
      struct thread *next_running_thread = list_entry (list_begin (&ready_list), struct thread, elem);
      if (cur->priority < next_running_thread->priority)
      {
        /* Yield for the higher priority thread in ready list. */
        thread_yield();
      }
    }
  }
  intr_set_level (old_level);
}

/* Recursively donate priority. We need to use recursion to handle nested donations. */
void
thread_donate_priority(struct thread *t)
{
  if(thread_mlfqs) return;
  struct lock *l = t->wait_on_lock;
  
  /* Need to check l first and l->holder second, otherwise there might be Page Errors. */
  if (l)
  {
    if (l->holder)
    {
      /* Thread which holds the lock that the current thread is waiting for, this thread is a possible donee. */
      if (l->holder->priority >= t->priority) {
          return;
      }
      l->holder->priority = t->priority;
      thread_donate_priority (l->holder);
    }
  }
  else
  {
    /* No lock. The current thread should be already in the ready list. Sort the ready list by priorities. */
    list_sort (&ready_list, higher_priority_fun, 0);
  }
}

void
remove_donor_threads_associate_with_lock (struct lock *lock)
{
  struct list_elem *e;
  struct thread *cur = thread_current ();

  for (e = list_begin (&cur->donor_threads); e != list_end (&cur->donor_threads);
    e = list_next (e))
  {
    struct thread *t = list_entry (e, struct thread, donor_thread_elem);
    if (t->wait_on_lock == lock)
    {
      list_remove(e);
    }
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  struct thread *t = thread_current();
  // update nice value 
  t->nice = nice;

  // update priority
  int priority = PRI_MAX - (t->recent_cpu / 4) / f - (t->nice * 2);
  priority = priority > 63 ? 63 : priority;
  priority = priority < 0 ? 0 : priority;
  t->priority = priority;
    
  struct list_elem *front;
  if (!list_empty(&ready_list))
  {
    front = list_front(&ready_list);
    struct thread *p = list_entry(front, struct thread, elem);
    if (p->priority > priority) {
      thread_yield();
      // intr_yield_on_return ();
    }
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  // intr_disable();
  enum intr_level old_level;

  // ASSERT (is_thread (t));

  old_level = intr_disable ();
  int load_return = ((100 * load_avg + f/2)) / f;
  // int load_return = fixedpoint_to_int( fixedpoint_mul_int(load_avg, 100) );
  //  
  // printf("load_avg: %d, f: %d, load average after: %d\n", load_avg, f, load_return);
  // intr_enable();
  intr_set_level (old_level);
  return load_return;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level old_level = intr_disable();
  int cpu_rec = thread_current()->recent_cpu;
  int recent_cpu_return = ((100 * cpu_rec) + f/2) / f;
  intr_set_level(old_level);
  return recent_cpu_return;
}

/* Idle thread.  Executes when no other thread is ready to run.
   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.
         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.
         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->new_priority = priority;
  t->magic = THREAD_MAGIC;
  t->wait_on_lock = NULL;
  
  old_level = intr_disable ();
  list_init(&t->donor_threads);
  list_push_back (&all_list, &t->allelem);

  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.
   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).
   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.
   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
  // printf("finish schedule thread_schedule_tail\n");
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);