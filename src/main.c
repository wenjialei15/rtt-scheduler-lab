/*
 * Scheduling and IPC experiments on the real RT-Thread 4.1.1 kernel,
 * running on the Windows simulator port.
 */

#include <rthw.h>
#include <rtthread.h>
#include <stdio.h>

extern int rtthread_startup(void);

/* Provided by port/board.c */
extern rt_uint64_t rt_hw_bench_us(void);

#define STACK_SIZE      8192

/* ------------------------------------------------------------------------- */
/* E1: does a higher priority thread preempt a running lower one?            */
/* ------------------------------------------------------------------------- */

static rt_sem_t e1_go_low, e1_go_mid, e1_go_high;
static char e1_log[64];
static int e1_len;

static void e1_append(char c)
{
    if (e1_len < (int)sizeof(e1_log) - 1)
    {
        e1_log[e1_len++] = c;
        e1_log[e1_len] = '\0';
    }
}

static void e1_worker(void *arg)
{
    char id = (char)(rt_ubase_t)arg;
    rt_sem_t go = (id == 'L') ? e1_go_low : (id == 'M') ? e1_go_mid : e1_go_high;
    rt_tick_t start;

    rt_sem_take(go, RT_WAITING_FOREVER);
    e1_append(id);

    start = rt_tick_get();
    while (rt_tick_get() - start < 3)
    {
        /* busy wait, keep the CPU */
    }
    e1_append((char)(id + 32));     /* lowercase = finished */
}

static void e1_run(void)
{
    rt_thread_t low, mid, high;

    e1_len = 0;
    e1_log[0] = '\0';
    e1_go_low = rt_sem_create("gL", 0, RT_IPC_FLAG_FIFO);
    e1_go_mid = rt_sem_create("gM", 0, RT_IPC_FLAG_FIFO);
    e1_go_high = rt_sem_create("gH", 0, RT_IPC_FLAG_FIFO);

    low = rt_thread_create("low", e1_worker, (void *)'L', STACK_SIZE, 16, 10);
    mid = rt_thread_create("mid", e1_worker, (void *)'M', STACK_SIZE, 12, 10);
    high = rt_thread_create("high", e1_worker, (void *)'H', STACK_SIZE, 8, 10);

    rt_thread_startup(low);
    rt_thread_startup(mid);
    rt_thread_startup(high);

    /* Release in the "wrong" order on purpose: low, then mid, then high. */
    rt_sem_release(e1_go_low);
    rt_thread_mdelay(1);
    rt_sem_release(e1_go_mid);
    rt_thread_mdelay(1);
    rt_sem_release(e1_go_high);

    rt_thread_mdelay(40);

    rt_kprintf("E1 preemption   log=%s  (expect L M H h m l)\n", e1_log);
}

/* ------------------------------------------------------------------------- */
/* E2: two threads at the same priority, do they get a fair time slice?      */
/* ------------------------------------------------------------------------- */

static volatile rt_uint32_t e2_a, e2_b;
static rt_sem_t e2_done;

static void e2_worker(void *arg)
{
    volatile rt_uint32_t *counter = (volatile rt_uint32_t *)arg;
    rt_tick_t start = rt_tick_get();

    while (rt_tick_get() - start < 100)
    {
        (*counter)++;
    }
    rt_sem_release(e2_done);
}

static void e2_run(void)
{
    rt_thread_t a, b;

    e2_a = e2_b = 0;
    e2_done = rt_sem_create("d2", 0, RT_IPC_FLAG_FIFO);

    a = rt_thread_create("sliceA", e2_worker, (void *)&e2_a, STACK_SIZE, 12, 10);
    b = rt_thread_create("sliceB", e2_worker, (void *)&e2_b, STACK_SIZE, 12, 10);
    rt_thread_startup(a);
    rt_thread_startup(b);

    rt_sem_take(e2_done, RT_WAITING_FOREVER);
    rt_sem_take(e2_done, RT_WAITING_FOREVER);

    rt_kprintf("E2 time slice   A=%u B=%u  A/B=%d.%02d\n", e2_a, e2_b,
               (e2_b == 0) ? 0 : (int)((rt_uint64_t)e2_a * 100 / e2_b / 100),
               (e2_b == 0) ? 0 : (int)((rt_uint64_t)e2_a * 100 / e2_b % 100));
}


/* ------------------------------------------------------------------------- */
/* E3: priority inheritance - can a mid-priority thread starve a low one?    */
/* ------------------------------------------------------------------------- */

static rt_mutex_t e3_mtx;
static char e3_log[128];
static int e3_len;

static void e3_add(const char *tag)
{
    int n = rt_strlen(tag);
    if (e3_len + n + 2 < (int)sizeof(e3_log))
    {
        rt_memcpy(&e3_log[e3_len], tag, n);
        e3_len += n;
        e3_log[e3_len++] = ' ';
        e3_log[e3_len] = '\0';
    }
}

static void e3_low(void *arg)
{
    rt_tick_t start;

    e3_add("L+");
    rt_mutex_take(e3_mtx, RT_WAITING_FOREVER);
    start = rt_tick_get();
    while (rt_tick_get() - start < 10) { }
    rt_mutex_release(e3_mtx);
    e3_add("L-");
}

static void e3_high(void *arg)
{
    rt_thread_mdelay(1);
    e3_add("H?");
    rt_mutex_take(e3_mtx, RT_WAITING_FOREVER);
    e3_add("H+");
    rt_mutex_release(e3_mtx);
}

static void e3_mid(void *arg)
{
    rt_tick_t start;

    rt_thread_mdelay(2);
    e3_add("M+");
    start = rt_tick_get();
    while (rt_tick_get() - start < 10) { }
    e3_add("M-");
}

static void e3_run(void)
{
    rt_thread_t low, mid, high;

    e3_len = 0;
    e3_log[0] = '\0';
    e3_mtx = rt_mutex_create("m3", RT_IPC_FLAG_PRIO);

    low  = rt_thread_create("e3low",  e3_low,  RT_NULL, STACK_SIZE, 16, 10);
    mid  = rt_thread_create("e3mid",  e3_mid,  RT_NULL, STACK_SIZE, 12, 10);
    high = rt_thread_create("e3high", e3_high, RT_NULL, STACK_SIZE, 8, 10);
    rt_thread_startup(low);
    rt_thread_startup(mid);
    rt_thread_startup(high);

    rt_thread_mdelay(40);
    rt_kprintf("E3 inherit      order=%s (expect L+ H? L- H+ M+ M-)\n", e3_log);
}

/* ------------------------------------------------------------------------- */
/* E4: mailbox round trip between two threads                                */
/* ------------------------------------------------------------------------- */

#define E4_ROUNDS       500

static rt_mailbox_t e4_ping, e4_pong;

static void e4_echo(void *arg)
{
    rt_ubase_t value;
    int i;

    for (i = 0; i < E4_ROUNDS; i++)
    {
        rt_mb_recv(e4_ping, &value, RT_WAITING_FOREVER);
        rt_mb_send(e4_pong, value);
    }
}

static void e4_run(void)
{
    rt_thread_t echo;
    rt_ubase_t value = 0;
    rt_uint64_t t0, t1;
    int i;

    e4_ping = rt_mb_create("ping", 4, RT_IPC_FLAG_PRIO);
    e4_pong = rt_mb_create("pong", 4, RT_IPC_FLAG_PRIO);

    echo = rt_thread_create("echo", e4_echo, RT_NULL, STACK_SIZE, 12, 10);
    rt_thread_startup(echo);

    t0 = rt_hw_bench_us();
    for (i = 0; i < E4_ROUNDS; i++)
    {
        rt_mb_send(e4_ping, 1);
        rt_mb_recv(e4_pong, &value, RT_WAITING_FOREVER);
    }
    t1 = rt_hw_bench_us();

    rt_kprintf("E4 mailbox      %d round trips, %d us total, %d us each\n",
               E4_ROUNDS, (int)(t1 - t0), (int)((t1 - t0) / E4_ROUNDS));
}

/* ------------------------------------------------------------------------- */
/* E5: cost of a context switch (yield)                                      */
/* ------------------------------------------------------------------------- */

#define E5_YIELDS       5000

static rt_sem_t e5_done;
static volatile rt_uint32_t e5_us;

static void e5_worker(void *arg)
{
    rt_uint64_t t0 = rt_hw_bench_us();
    int i;

    for (i = 0; i < E5_YIELDS; i++)
    {
        rt_thread_yield();
    }
    e5_us = (rt_uint32_t)(rt_hw_bench_us() - t0);
    rt_sem_release(e5_done);
}

static void e5_run(void)
{
    rt_thread_t t;

    e5_done = rt_sem_create("d5", 0, RT_IPC_FLAG_FIFO);
    t = rt_thread_create("yield", e5_worker, RT_NULL, STACK_SIZE, 10, 10);
    rt_thread_startup(t);

    rt_sem_take(e5_done, RT_WAITING_FOREVER);
    rt_kprintf("E5 switch       %d yields in %d us, %d.%02d us per switch\n",
               E5_YIELDS, (int)e5_us, (int)(e5_us / E5_YIELDS),
               (int)((e5_us % E5_YIELDS) * 100 / E5_YIELDS));
}

static void app_run(void)
{
    rt_kprintf("\n=== RT-Thread scheduling / IPC lab ===\n");
    rt_kprintf("kernel %d.%d.%d, tick %d Hz, %d priorities\n\n",
               RT_VERSION, RT_SUBVERSION, RT_REVISION,
               RT_TICK_PER_SECOND, RT_THREAD_PRIORITY_MAX);

    e1_run();
    e2_run();
    /* e3_run(): priority inheritance hangs on the win32 port, see README */
    e4_run();
    e5_run();

    rt_kprintf("\nRESULT preemption=%s\n", e1_log);
    rt_kprintf("RESULT slice_a=%u slice_b=%u\n", e2_a, e2_b);
    rt_kprintf("\ndone\n");
}

int main(void)
{
    static int started = 0;

    if (!started)
    {
        started = 1;
        rtthread_startup();
    }

    app_run();
    return 0;
}
