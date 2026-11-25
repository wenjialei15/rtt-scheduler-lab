# rtt-scheduler-lab

把 **RT-Thread 4.1.1 的真实内核**编译到 Windows 上运行，用它自己的调度器做四组可重复的测量。不需要开发板，也不需要 Linux。

```
kernel 4.1.1, tick 100 Hz, 32 priorities

E1 preemption   log=LMHhml  (expect L M H h m l)
E2 time slice   A=778263413 B=639980371  A/B=1.21
E4 mailbox      500 round trips, 24672 us total, 49 us each
E5 switch       5000 yields in 26527 us, 5.30 us per switch
```

三次运行的结果：

| 实验 | 结果 | 稳定性 |
|---|---|---|
| E1 抢占顺序 | `LMHhml` | 3/3 一致 |
| E2 同优先级时间片 | A/B = 1.12 / 1.18 / 1.21（另有一次 0.82） | 有明显波动 |
| E4 邮箱往返 | 45 / 49 / 55 µs | ±10% |
| E5 一次让出（上下文切换） | 4.89 / 4.95 / 5.32 µs | ±5% |

完整输出见 [`docs/results.txt`](docs/results.txt)。

## 为什么要做

"会 RTOS"在简历上通常是一句空话。能讲清楚的只有三种：用过某个 RTOS 做产品、读过它的源码、**在它上面量过东西**。

这个项目做第三种：把官方内核原样编进一个 Windows 进程，用真实的 `rt_thread_create` / `rt_sem` / `rt_mailbox` 跑实验，所有的数字都是 RT-Thread 自己的调度器产生的。

## 怎么做的

```
src/main.c              四组实验
      │
      ▼
RT-Thread src/*.c       ← 上游内核，一行未改
libcpu/sim/win32/*.c    ← 上游 Windows 移植层（线程 + SuspendThread 切换）
      │
      ▼
port/board.c            ← 本仓库：8 MB 堆、WriteFile 控制台、QPC 微秒时钟
port/rtconfig.h         ← 最小配置：只有内核 + IPC，没有 DFS / finsh
```

两个关键点：

1. **必须编译成 32 位**（`-target x86-windows-gnu`）。RT-Thread 的 `rt_base_t` 定义成 `long`，而 Windows 64 位下 `long` 是 32 位，直接编 64 位会把指针截断，启动就崩。
2. **主线程栈不能小于 8 KB**。4 KB 时 `rt_kprintf` 在第一个线程里就崩，因为 `rt_vsnprintf` 的栈用量比裸机大。

## 四组实验

**E1 抢占**：低、中、高三个线程（优先级 16 / 12 / 8）各忙 3 个 tick，故意按低→中→高的顺序释放信号量。

结果 `LMHhml`：低先跑，被中抢占，再被高抢占，然后按高→中→低的顺序结束。抢占是真的。

**E2 时间片**：两个同为优先级 12 的线程各忙 100 个 tick，比较循环次数。

A/B 在 1.1 左右，但没有严格的 1:1——见下面的边界。

**E4 邮箱往返**：两个线程用两个邮箱 ping-pong 500 次，用 `QueryPerformanceCounter` 计时，约 50 µs 一次往返（含两次线程切换和两次邮箱操作）。

**E5 切换开销**：单线程连续 `rt_thread_yield()` 5000 次，约 5 µs 一次。

## 边界

- **这是模拟器，不是真机**。RT-Thread 线程被映射成 Windows 线程，切换靠 `SuspendThread` / `ResumeThread`，所以绝对数字（5 µs、50 µs）只在这个环境下成立，不能代表 Cortex-M 上的表现。
- **E2 的 ±20% 波动**来自宿主线程调度和模拟器的切换延迟，不是 RT-Thread 的时间片算法有问题。
- **优先级继承没有测出来**：E3 跑到 `L+ H? H+` 就停了，`L-`、`M+`、`M-` 都没出现，怀疑是移植层在改线程优先级时把线程挂起后没有恢复。这一项没有纳入结论，代码留在 `src/main.c` 里，注释掉了。
- 只编了 32 位；64 位需要先把 `rt_base_t` 改成 64 位类型。

## 构建

需要 [zig](https://ziglang.org)（只用它当 C 编译器，不写 Zig 代码）：

```bash
git submodule update --init third_party/rt-thread
./build.sh
./build/rtt.exe
```

`build.sh` 里的核心命令：

```bash
zig cc -target x86-windows-gnu -Iport -Ithird_party/rt-thread/include \
  third_party/rt-thread/src/*.c \
  third_party/rt-thread/libcpu/sim/win32/cpu_port.c \
  port/board.c src/main.c -lwinmm -o build/rtt.exe
```

程序跑完实验后不会退出（idle 线程在跑），`Ctrl+C` 结束即可。

## 许可

Apache-2.0。`third_party/rt-thread` 版权归 RT-Thread 团队所有。
