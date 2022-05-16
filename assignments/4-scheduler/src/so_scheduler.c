#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <semaphore.h>

#include "so_scheduler.h"
#include "utils.h"
#include "priority_queue.h"
#include "linkedlist.h"

#define ERROR -1

/*
 * possible states of a thread
 */
/*
#define NEW 0
#define READY 1
#define RUNNING 2
#define WAITING 3
#define TERMINATED 4
*/

#define OK 1
#define NOT_READY 0

#define NO_IO_WAITED SO_MAX_NUM_EVENTS

typedef struct so_task {
    /*
     * handler function
     */
    so_handler *handler;
    unsigned int priority;
    // int state;
    pthread_t tid;
    sem_t *sem;

} so_task_t;

typedef struct so_scheduler {
    int state;
    /*
     * time quantum for each thread
     */
    unsigned int time_quantum;
    /*
     * number of IO devices supported
     */
    unsigned int io;

    /*
     * priority queue of ready threads
     */
    // node_t *ready_pq;

    /*
     * hashmap with threads, based on priority
     */
    node_linkedlist_t *ready[SO_MAX_PRIO + 1];
    /*
     * current running thread
     */
    so_task_t *running;
    int current_quantum;
    /*
     * linked list with waiting threads
     */
    // node_linkedlist_t *waiting;
    node_linkedlist_t *waiting[SO_MAX_NUM_EVENTS];
    node_linkedlist_t *terminated;
    int total_threads;
} so_scheduler_t;

static so_scheduler_t scheduler;

/*
 * creates and initializes scheduler
 * + time quantum for each thread
 * + number of IO devices supported
 * returns: 0 on success or negative on error
 */
DECL_PREFIX int so_init(unsigned int time_quantum, unsigned int io)
{
    if (time_quantum < 0 || io < 0 || io >= SO_MAX_NUM_EVENTS)
        return ERROR;

    scheduler.time_quantum = time_quantum;
    scheduler.io = io;
    // scheduler.ready_pq = NULL;
    scheduler.running = NULL;
    scheduler.current_quantum = time_quantum;
    // scheduler.waiting = NULL;
    scheduler.state = OK;
    scheduler.terminated = NULL;
    scheduler.total_threads = 0;

    return 0;
}

void schedule()
{
    so_task_t *best_candidate = NULL, *crt_running;
    node_linkedlist_t *best_candidate_node = NULL;
    int i;

    crt_running = scheduler.running;
    scheduler.current_quantum--;
    
    for (i = SO_MAX_PRIO; i >= 0; i--) {
        if (scheduler.ready[i] != NULL) {
            best_candidate_node = scheduler.ready[i];
            best_candidate = best_candidate_node->data;
            scheduler.ready[i] = scheduler.ready[i]->next;
            break;
        }
    }

    /*
     * if the current running thread needs to be changed
     */
    if (best_candidate != NULL &&
        (crt_running == NULL ||
        crt_running->priority < best_candidate->priority ||
        (crt_running->priority == best_candidate->priority &&
        scheduler.current_quantum <= 0))) {
        scheduler.running = best_candidate;
        if (crt_running)
            DIE(insert_list(&scheduler.ready[crt_running->priority],
                            crt_running),
                "inserting into linkedlist failed");
        free(best_candidate_node);
        scheduler.current_quantum = scheduler.time_quantum;

    /*
     * if the current running thread will continue to run
     */
    } else if (best_candidate == NULL ||
                (crt_running != NULL &&
                (crt_running->priority > best_candidate->priority ||
                (crt_running->priority == best_candidate->priority &&
                scheduler.current_quantum > 0)))) {
        scheduler.running = crt_running;
        if (scheduler.current_quantum <= 0)
            scheduler.current_quantum = scheduler.time_quantum;
        if (best_candidate)
            scheduler.ready[best_candidate->priority] = best_candidate_node;
    }
}

void *start_thread(void *params)
{
    so_task_t *crt_thread = (so_task_t *)params;

    /*
     * așteaptă să fie planificat
     */
    sem_wait(crt_thread->sem);

    crt_thread->handler(crt_thread->priority);

    insert_list_front(&scheduler.terminated, crt_thread);

    return NULL;
}

/*
 * creates a new so_task_t and runs it according to the scheduler
 * + handler function
 * + priority
 * returns: tid of the new task if successful or INVALID_TID
 */
DECL_PREFIX tid_t so_fork(so_handler *func, unsigned int priority)
{
    so_task_t *thread;

    /*
     * check params
     */
    if (func == NULL || priority < 0 || priority > SO_MAX_PRIO)
        return INVALID_TID;

    /*
     * initialize thread struct (so_task_t)
     */
    thread = malloc(sizeof(so_task_t));
    if (thread == NULL)
        return INVALID_TID;

    thread->handler = func;
    thread->priority = priority;
    // thread->state = NEW;
    // thread->waited_io = NO_IO_WAITED;

    sem_init(thread->sem, 0, 0);

    if (pthread_create(&thread->tid, NULL, start_thread, thread))
        return INVALID_TID;
    scheduler.total_threads++;

    /*
     * mark thread as ready
     */
    // push(&scheduler.ready_pq, thread, priority);
    if (insert_list(&scheduler.ready[priority], thread))
        return INVALID_TID;

    schedule();

    return thread->tid;
}

/*
 * waits for an IO device
 * + device index
 * returns: -1 if the device does not exist or 0 on success
 */
DECL_PREFIX int so_wait(unsigned int io)
{
    so_task_t *thread = scheduler.running;

    if (io < 0 || io >= scheduler.io)
        return -1;

    // scheduler.running->waited_io = io;

    if (insert_list(&scheduler.waiting[io], scheduler.running))
        return -1;

    scheduler.running = NULL;

    sem_wait(thread->sem);
    // scheduler.running = NULL;
    // schedule();

    return 0;
}

int wake_up_threads(unsigned int io)
{
    node_linkedlist_t *last[SO_MAX_PRIO + 1], *next_ready, *p;
    int i, woken_threads = 0;
    so_task_t *thread;

    for (i = 0; i <= SO_MAX_PRIO; i++)
        last[i] = NULL;

    while (!empty_list(scheduler.waiting[io])) {
        next_ready = scheduler.waiting[io];
        thread = next_ready->data;
        scheduler.waiting[io] = next_ready->next;

        if (!last[thread->priority]) {
            p = scheduler.ready[thread->priority];

            for ( ; p && p->next; p = p->next);

            last[thread->priority] = p;
        }

        if (!last[thread->priority])
            scheduler.ready[thread->priority] = next_ready;
        else
            last[thread->priority]->next = next_ready;

        last[thread->priority] = next_ready;
        next_ready->next = NULL;
        woken_threads++;
    }

    return woken_threads;
}

/*
 * signals an IO device
 * + device index
 * return the number of tasks woke or -1 on error
 */
DECL_PREFIX int so_signal(unsigned int io)
{
    // node_linkedlist_t *p;
    int woken_tasks = 0;
    // so_task_t *thread;

    if (io < 0 || io >= scheduler.io)
        return -1;

    // scheduler.current_quantum--;

    woken_tasks = wake_up_threads(io);

    schedule();

    return woken_tasks;
}

/*
 * does whatever operation
 */
DECL_PREFIX void so_exec(void)
{
    scheduler.current_quantum--;

    schedule();
}

/*
 * destroys a scheduler
 */
DECL_PREFIX void so_end(void)
{
    node_linkedlist_t *p;
    so_task_t *thread;

    if (scheduler.state == NOT_READY)
        return;
    scheduler.state = NOT_READY;

    while (scheduler.total_threads) {
        p = pop_list(&scheduler.terminated);
        thread = p->data;

        if (thread) {
            pthread_join(thread->tid, NULL);
            sem_destroy(thread->sem);
            free(thread);
            scheduler.total_threads--;
        }
    }
}