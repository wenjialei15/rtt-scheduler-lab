/*
 * Minimal board port for the Windows simulator.
 * Console goes straight to the Win32 console, so no device framework is needed.
 */

#include <rthw.h>
#include <rtthread.h>

#include <windows.h>
#include <stdlib.h>

#define RT_HEAP_SIZE        (8 * 1024 * 1024)

void rt_hw_console_output(const char *str)
{
    DWORD written;
    DWORD len = (DWORD)rt_strlen(str);

    /* WriteFile does not touch the CRT or the stack probe, which matters
     * because RT-Thread threads run on stacks it allocated itself. */
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), str, len, &written, NULL);
}

int rt_hw_board_init(void)
{
    static rt_uint8_t *heap = RT_NULL;


    if (heap == RT_NULL)
    {
        heap = (rt_uint8_t *)malloc(RT_HEAP_SIZE);
    }
    if (heap == RT_NULL)
    {
        return -1;
    }

#ifdef RT_USING_HEAP
    rt_system_heap_init((void *)heap, (void *)(heap + RT_HEAP_SIZE - 1));
#endif
    return 0;
}

/* Microsecond clock for the benchmarks (independent of the RT-Thread tick). */
rt_uint64_t rt_hw_bench_us(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER now;

    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&now);

    return (rt_uint64_t)((now.QuadPart * 1000000ULL) / freq.QuadPart);
}
