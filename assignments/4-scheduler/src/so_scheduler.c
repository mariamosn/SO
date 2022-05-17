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
    pthread_t tid;
    sem_t sem;

} so_task_t;

typedef struct so_scheduler {
    /*
     * the current state of the scheduler
     * (initialized or not)
     */
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
     * hashmap with threads, based on priority
     */
    node_linkedlist_t *ready[SO_MAX_PRIO + 1];
    /*
     * current running thread
     */
    so_task_t *running;
    /*
     * time left for the current running thread
     */
    int current_quantum;
    /*
     * linked lists with waiting threads,
     * based on priorities
     */
    node_linkedlist_t *waiting[SO_MAX_NUM_EVENTS];
    /*
     * linked list with all the terminated threads
     */
    node_linkedlist_t *terminated;
    // int total_threads;
    /*
     * TSD: data about the current thread
     */
    pthread_key_t current_thread;
    /*
     * thread responsible for so_end
     */
    so_task_t *ending_thread;
} so_scheduler_t;

so_scheduler_t scheduler;

int needs_to_be_replaced(so_task_t *candidate)
{
    /*
     * the previously running thread is done and there
     * are still other ready threads
     */
    if (!scheduler.running && candidate)
        return 1;

    /*
     * there are no ready threads left
     */
    if (!candidate)
        return 0;

    /*
     * the next running thread must have the max priority available
     */
    if (scheduler.running->priority < candidate->priority)
        return 1;

    if (scheduler.running->priority > candidate->priority)
        return 0;

    /*
     * if the time for the current running thread expired and there is
     * another suitable candidate, it will replace the current thread
     */
    if (scheduler.current_quantum <= 0)
        return 1;

    return 0;
}

void schedule(int wait)
{
    so_task_t *best_candidate = NULL, *crt_running, *crt_thread;
    node_linkedlist_t *best_candidate_node = NULL;
    int i;

    crt_running = scheduler.running;
    crt_thread = pthread_getspecific(scheduler.current_thread);
    scheduler.current_quantum--;
    
    for (i = SO_MAX_PRIO; i >= 0; i--) {
        if (scheduler.ready[i] != NULL) {
            best_candidate_node = scheduler.ready[i];
            best_candidate = best_candidate_node->data;
            scheduler.ready[i] = scheduler.ready[i]->next;
            break;
        }
    }

    if (needs_to_be_replaced(best_candidate)) {
        scheduler.running = best_candidate;
        if (crt_running)
            DIE(insert_list(&scheduler.ready[crt_running->priority],
                            crt_running),
                "inserting into linkedlist failed");
        free(best_candidate_node);
        scheduler.current_quantum = scheduler.time_quantum;
        sem_post(&scheduler.running->sem);
        if (wait)
            sem_wait(&crt_thread->sem);
    } else if (scheduler.running) {
        if (scheduler.current_quantum <= 0)
            scheduler.current_quantum = scheduler.time_quantum;
        if (best_candidate)
            scheduler.ready[best_candidate->priority] = best_candidate_node;
    } else {
        sem_post(&scheduler.ending_thread->sem);
        // sem_wait(&crt_thread->sem);
    }
}

/*
 * creates and initializes scheduler
 * + time quantum for each thread
 * + number of IO devices supported
 * returns: 0 on success or negative on error
 */
DECL_PREFIX int so_init(unsigned int time_quantum, unsigned int io)
{
    int i;
    so_task_t *thread;

    /*
     * check params
     */
    if (time_quantum == 0 || io > SO_MAX_NUM_EVENTS)
        return ERROR;

    /*
     * check if the scheduler is already initialized
     */
    if (scheduler.state == OK)
        return ERROR;

    /*
     * setup scheduler
     */
    scheduler.time_quantum = time_quantum;
    scheduler.io = io;

    for (i = 0; i <= SO_MAX_PRIO; i++) {
        scheduler.ready[i] = NULL;
    }

    /*
     * set info of the running (current) thread
     */
    thread = malloc(sizeof(so_task_t));
    if (thread == NULL)
        return ERROR;
    thread->handler = NULL;
    thread->priority = 0;
    thread->tid = pthread_self();
    sem_init(&thread->sem, 0, 0);
    scheduler.running = thread;

    // TODO ... - 1?
    scheduler.current_quantum = time_quantum;

    for (i = 0; i < SO_MAX_NUM_EVENTS; i++) {
        scheduler.waiting[i] = NULL;
    }

    scheduler.terminated = NULL;
    // scheduler.total_threads = 1;

    /*
     * add info about the current thread
     */
    pthread_key_create(&scheduler.current_thread, NULL);
    pthread_setspecific(scheduler.current_thread, thread);

    scheduler.ending_thread = NULL;

    scheduler.state = OK;

    schedule(1);

    return 0;
}

void *start_thread(void *params)
{
    so_task_t *crt_thread = (so_task_t *)params;

    /*
     * așteaptă să fie planificat
     */
    sem_wait(&crt_thread->sem);

    pthread_setspecific(scheduler.current_thread, crt_thread);

    crt_thread->handler(crt_thread->priority);

    // if (scheduler.running == crt_thread)
        // scheduler.running = NULL;

    insert_list_front(&scheduler.terminated, crt_thread);

    scheduler.running = NULL;
    schedule(0);

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
    so_task_t *forked_thread;

    /*
     * check params
     */
    if (func == NULL || priority > SO_MAX_PRIO)
        return INVALID_TID;

    /*
     * initialize thread struct (so_task_t)
     */
    forked_thread = malloc(sizeof(so_task_t));
    if (forked_thread == NULL)
        return INVALID_TID;

    forked_thread->handler = func;
    forked_thread->priority = priority;
    sem_init(&forked_thread->sem, 0, 0);

    if (pthread_create(&forked_thread->tid, NULL, start_thread, forked_thread))
        return INVALID_TID;
    // scheduler.total_threads++;

    /*
     * mark thread as ready
     */
    if (insert_list(&scheduler.ready[priority], forked_thread))
        return INVALID_TID;

    schedule(1);

    return forked_thread->tid;
}

/*
 * waits for an IO device
 * + device index
 * returns: -1 if the device does not exist or 0 on success
 */
DECL_PREFIX int so_wait(unsigned int io)
{
    // so_task_t *thread = pthread_getspecific(scheduler.current_thread);
    so_task_t *thread = scheduler.running;

    if (io < 0 || io >= scheduler.io)
        return -1;

    if (insert_list(&scheduler.waiting[io], thread))
        return -1;
    scheduler.running = NULL;

    schedule(0);

    sem_wait(&thread->sem);

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
    int woken_tasks;
    // so_task_t *thread = pthread_getspecific(scheduler.current_thread);

    if (io < 0 || io >= scheduler.io)
        return -1;

    woken_tasks = wake_up_threads(io);

    schedule(1);

    return woken_tasks;
}

/*
 * does whatever operation
 */
DECL_PREFIX void so_exec(void)
{
    // scheduler.current_quantum--;

    schedule(1);
}

/*
 * destroys a scheduler
 */
DECL_PREFIX void so_end(void)
{
    node_linkedlist_t *p;
    so_task_t *thread;
    int i;

    if (scheduler.state == NOT_READY)
        return;
    scheduler.state = NOT_READY;

    scheduler.ending_thread = pthread_getspecific(scheduler.current_thread);

    for (i = SO_MAX_PRIO; i >= 0; i--) {
        while (scheduler.ready[i]) {
            p = scheduler.ready[i];
            scheduler.running = scheduler.ready[i]->data;
            scheduler.ready[i] = scheduler.ready[i]->next;
            free(p);
            sem_post(&scheduler.running->sem);
            sem_wait(&scheduler.ending_thread->sem);
        }
    }

    // while (scheduler.total_threads > 1) {
    while (scheduler.terminated) {
        p = pop_list(&scheduler.terminated);
        thread = p->data;
        free(p);

        if (thread) {
            pthread_join(thread->tid, NULL);
            sem_destroy(&thread->sem);
            free(thread);
            // scheduler.total_threads--;
        }
    }
/*
    if (scheduler.running) {
        sem_destroy(&scheduler.running->sem);
        free(scheduler.running);
    }
*/
    sem_destroy(&scheduler.ending_thread->sem);
    free(scheduler.ending_thread);
    pthread_key_delete(scheduler.current_thread);
}