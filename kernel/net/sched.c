#include "net/sched.h"

void nx_net_work_init(struct nx_net_work *work, void (*function)(void *), void *argument)
{
    *work = (struct nx_net_work){ .function = function, .argument = argument };
}
void nx_net_queue_init(struct nx_net_queue *queue, size_t limit)
{
    *queue = (struct nx_net_queue){ .limit = limit };
}
enum nx_net_put nx_net_queue_put(struct nx_net_queue *queue, struct nx_net_work *work)
{
    if (work->queued) { ++queue->coalesced; return NX_NET_PUT_PENDING; }
    if (queue->count >= queue->limit) { ++queue->overflows; return NX_NET_PUT_FULL; }
    work->next = NULL;
    work->queued = true;
    if (queue->tail) queue->tail->next = work; else queue->head = work;
    queue->tail = work;
    ++queue->count;
    ++queue->enqueued;
    return NX_NET_PUT_QUEUED;
}
struct nx_net_work *nx_net_queue_take(struct nx_net_queue *queue)
{
    struct nx_net_work *work = queue->head;
    if (!work) return NULL;
    queue->head = work->next;
    if (!queue->head) queue->tail = NULL;
    work->next = NULL;
    work->queued = false;
    --queue->count;
    ++queue->taken;
    return work;
}
bool nx_net_queue_remove(struct nx_net_queue *queue, struct nx_net_work *work)
{
    if (!work->queued) return false;
    struct nx_net_work **link = &queue->head, *previous = NULL;
    while (*link && *link != work) { previous = *link; link = &(*link)->next; }
    if (!*link) return false;   /* queued on another queue */
    *link = work->next;
    if (queue->tail == work) queue->tail = previous;
    work->next = NULL;
    work->queued = false;
    --queue->count;
    return true;
}

void nx_net_timer_init(struct nx_net_timer *timer, void (*function)(void *), void *argument)
{
    *timer = (struct nx_net_timer){ .function = function, .argument = argument };
}
void nx_net_timers_init(struct nx_net_timers *timers)
{
    *timers = (struct nx_net_timers){ 0 };
}
static void unlink_timer(struct nx_net_timers *timers, struct nx_net_timer *timer)
{
    struct nx_net_timer **link = &timers->head;
    while (*link != timer) link = &(*link)->next;
    *link = timer->next;
    timer->next = NULL;
    timer->pending = false;
}
void nx_net_timer_arm(struct nx_net_timers *timers, struct nx_net_timer *timer,
                      uint64_t now, uint64_t delay)
{
    if (timer->pending) unlink_timer(timers, timer);
    timer->deadline = delay > UINT64_MAX - now ? UINT64_MAX : now + delay;
    struct nx_net_timer **link = &timers->head;
    while (*link && (*link)->deadline <= timer->deadline) link = &(*link)->next;
    timer->next = *link;
    *link = timer;
    timer->pending = true;
    ++timers->armed;
}
bool nx_net_timer_cancel(struct nx_net_timers *timers, struct nx_net_timer *timer)
{
    if (!timer->pending) return false;
    unlink_timer(timers, timer);
    ++timers->cancelled;
    return true;
}
bool nx_net_timers_next(const struct nx_net_timers *timers, uint64_t *deadline)
{
    if (!timers->head) return false;
    *deadline = timers->head->deadline;
    return true;
}
size_t nx_net_timers_run(struct nx_net_timers *timers, uint64_t now, size_t budget)
{
    size_t ran = 0;
    while (ran < budget && timers->head && timers->head->deadline <= now) {
        struct nx_net_timer *timer = timers->head;
        timers->head = timer->next;
        timer->next = NULL;
        timer->pending = false;
        ++timers->expired;
        ++ran;
        timer->function(timer->argument);
    }
    return ran;
}
