# 练习

 对实验报告的要求：

- 基于markdown格式来完成，以文本方式为主
- 填写各个基本练习中要求完成的报告内容
- 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
- 列出你认为OS原理中很重要，但在实验中没有对应上的知识点

## 练习0：填写已有实验

本实验依赖实验2/3。请把你做的实验2/3的代码填入本实验中代码中有“LAB2”,“LAB3”的注释相应部分。

## 练习1：分配并初始化一个进程控制块（需要编码）

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。

- 【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

### alloc_proc函数实现过程

```cpp
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        // LAB4:EXERCISE1:  1913697唐俊杰 2210386吴邦宸 2212495姚明辰
        /*
         * below fields in proc_struct need to be initialized
         *       enum proc_state state;                      // Process state
         *       int pid;                                    // Process ID
         *       int runs;                                   // the running times of Proces
         *       uintptr_t kstack;                           // Process kernel stack
         *       volatile bool need_resched;                 // bool value: need to be rescheduled to release CPU?
         *       struct proc_struct *parent;                 // the parent process
         *       struct mm_struct *mm;                       // Process's memory management field
         *       struct context context;                     // Switch here to run process
         *       struct trapframe *tf;                       // Trap frame for current interrupt
         *       uintptr_t cr3;                              // CR3 register: the base addr of Page Directroy Table(PDT)
         *       uint32_t flags;                             // Process flag
         *       char name[PROC_NAME_LEN + 1];               // Process name
         */
        proc->state = PROC_UNINIT; // 未初始化
        proc->pid = -1;            // pid为-1
        proc->runs = 0;
        proc->kstack = 0;                                    // 除了idleproc其他线程的内核栈都要后续分配
        proc->need_resched = 0;                              // 不需要调度切换线程
        proc->parent = NULL;                                 //
        proc->mm = NULL;                                     // 未分配内存
        memset(&(proc->context), 0, sizeof(struct context)); // 清空上下文变量
        proc->tf = NULL;                                     // 初始化没有中断帧
        proc->cr3 = boot_cr3;                                // 内核线程的cr3为boot_cr3，即页目录为内核页目录表
        proc->flags = 0;                                     // 标志位为0
        memset(proc->name, 0, PROC_NAME_LEN + 1);            // 清空名称
    }
    return proc;
}

```

`alloc_proc()`负责分配创建一个 `proc_struct`并对其进行基本初始化，仅起到了创建进程块实例的作用，没有创建内核线程本身。

### 请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用

```cpp
int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags) {
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    // 设置内核线程的参数和函数指针
    tf.gpr.s0 = (uintptr_t)fn;      //s0 寄存器保存函数指针、函数入口
    tf.gpr.s1 = (uintptr_t)arg;     //s1 寄存器保存函数参数
    // 设置 trapframe 中的 status 寄存器（SSTATUS）
    // SSTATUS_SPP：Supervisor Previous Privilege（设置为 supervisor 模式，因为这是一个内核线程）
    // SSTATUS_SPIE：Supervisor Previous Interrupt Enable（设置为启用中断，因为这是一个内核线程）
    // SSTATUS_SIE：Supervisor Interrupt Enable（设置为禁用中断，因为我们不希望该线程被中断）
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;    //epc指向kernel_thread_entry，即执行s0指向的函数
    //// 使用 do_fork 创建一个新进程（内核线程），这样才真正用设置的tf创建新进程
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf) {
    //内核栈上分配一块空间保存tf
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
    *(proc->tf) = *tf;
    proc->tf->gpr.a0 = 0;               //a0设置为0表示为子进程
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;//esp非空则设置tf栈指针为esp,否则指向自己
    proc->context.ra = (uintptr_t)forkret;//上下文的ra设置为forkret入口
    proc->context.sp = (uintptr_t)(proc->tf);//上下文的栈顶放置tf
}
```

在 `kernel_thread()` 和 `copy_thread()` 函数中，我们可以看到对 `tf` 和 `context` 的操作。作为 `proc_struct` 结构体的一部分，`context` 保存了进程的上下文信息，即调用者保存的寄存器值，这使得 uCore 能够在内核态实现上下文切换和进程调度。而 `tf` 则是指向中断帧的指针，用于记录进程从用户空间切换到内核空间时的状态。无论是在进入内核时，还是在内核态创建新线程时，`tf` 中保存的中断前状态能够帮助恢复各个寄存器的值，从而使进程能够继续执行。

## 练习2：为新创建的内核线程分配资源（需要编码）

创建一个内核线程需要分配和设置好很多资源。

kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们**实际需要"fork"的东西就是stack和trapframe**。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

### do_fork函数实现过程

```cpp

int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    // LAB4:EXERCISE2 1913697唐俊杰 2210386吴邦宸 2212495姚明辰
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakeup_proc:  set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    proc = alloc_proc();
    if (proc == NULL)
    { // 分配失败
        goto fork_out;
    }
    proc->parent = current;
    //    2. call setup_kstack to allocate a kernel stack for child process
    if (setup_kstack(proc))
    {
        goto bad_fork_cleanup_kstack;
    }
    //    3. call copy_mm to dup OR share mm according clone_flag
    if (copy_mm(clone_flags, proc))
    {
        goto bad_fork_cleanup_proc;
    }
    //    4. call copy_thread to setup tf & context in proc_struct
    copy_thread(proc, stack, tf);
    //    5. insert proc_struct into hash_list && proc_list
    bool intr_flag;
    local_intr_save(intr_flag);

    // 获取进程号、加入列表
    proc->pid = get_pid();

    hash_proc(proc);
    list_add(&proc_list, &(proc->list_link));
    nr_process++;
    local_intr_restore(intr_flag);
    //    6. call wakeup_proc to make the new child process RUNNABLE
    wakeup_proc(proc);

    //    7. set ret vaule using child proc's pid
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

```

1. 检查当前进程数量是否已经达到最大限制（MAX_PROCESS），如果超过则直接跳转到fork_out标签处，返回-E_NO_FREE_PROC。
2. 设置返回值为-E_NO_MEM，以便在发生内存分配失败时可以返回适当的错误码。
3. 调用alloc_proc函数来分配一个进程控制块（proc_struct），并将返回的指针保存在proc变量中。如果分配失败，则直接跳转到fork_out标签处，返回-E_NO_MEM。
4. 将新进程的父进程指针指向当前进程。
5. 调用setup_kstack函数为新进程分配内核栈空间，如果失败则跳转到bad_fork_cleanup_kstack标签处进行清理工作。
6. 调用copy_mm函数复制原进程的内存管理信息到新进程，这一步骤是复制进程的内存布局信息，以确保新进程拥有与原进程相同的内存环境。如果失败，则跳转到bad_fork_cleanup_proc标签处进行清理工作。
7. 调用copy_thread函数复制原进程的上下文到新进程，包括栈和 trapframe。
8. 为新进程分配一个唯一的进程号（pid），调用get_pid函数实现此目的。该函数中含有静态全局变量，需要保证唯一性，避免多个进程同时执行附近操作导致的数值错误，需要将此步骤和下一步部分添加中断，防止同时调度。
9. 将新进程添加到进程列表，并更新进程数量计数器。
10. 唤醒新进程，使其进入可调度状态。
11. 返回新进程的pid作为函数的返回值。
12. 通过调用get_pid()函数为新进程分配一个唯一的pid。该函数实现了一个简单的计数器，并通过自增操作返回一个唯一的值作为pid。

### 请说明ucore是否做到给每个新fork的线程一个唯一的id

根据上述代码，uCore 在每次创建新的内核线程时，都会通过调用 `get_pid` 函数为新进程分配一个唯一的 PID，并将其赋值给新进程的 `proc->pid` 字段，从而确保每个新创建的进程具有唯一的 PID。

在 `get_pid` 函数中，首先判断 `last_pid` 是否小于 `next_safe`，如果是，那么分配的 `last_pid` 就是唯一的。若 `last_pid` 大于或等于 `next_safe`，或者已经超过了 `MAX_PID`，则需要遍历 `proc_list`，重新设置 `last_pid` 和 `next_safe`，以保证下一次 PID 分配时能够正常进行。

## 练习3：编写proc_run 函数（需要编码）

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：

- 检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
- 禁用中断。你可以使用 `/kern/sync/sync.h`中定义好的宏 `local_intr_save(x)`和 `local_intr_restore(x)`来实现关、开中断。
- 切换当前进程为要运行的进程。
- 切换页表，以便使用新进程的地址空间。`/libs/riscv.h`中提供了 `lcr3(unsigned int cr3)`函数，可实现修改CR3寄存器值的功能。
- 实现上下文切换。`/kern/process`中已经预先编写好了 `switch.S`，其中定义了 `switch_to()`函数。可实现两个进程的context切换。
- 允许中断。

请回答如下问题：

- 在本实验的执行过程中，创建且运行了几个内核线程？

完成代码编写后，编译并运行代码：make qemu

如果可以得到如 附录A所示的显示内容（仅供参考，不是标准答案输出），则基本正确。

### proc_run函数实现过程

```cpp

void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        // LAB4:EXERCISE3 1913697唐俊杰 2210386吴邦宸 2212495姚明辰
        /*
         * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
         * MACROs or Functions:
         *   local_intr_save():        Disable interrupts
         *   local_intr_restore():     Enable Interrupts
         *   lcr3():                   Modify the value of CR3 register
         *   switch_to():              Context switching between two processes
         */
        bool intr_flag;
        local_intr_save(intr_flag); // 禁用中断

        struct proc_struct *prev = current;

        current = proc;                                // 当前进程指针指向新进程
        lcr3(proc->cr3);                               // 切换到下一个进程的页表
        switch_to(&(prev->context), &(proc->context)); // 进行上下文切换

        local_intr_restore(intr_flag); // 启用中断
    }
}

```

### 在本实验的执行过程中，创建且运行了几个内核线程？

创建了两个内核线程：idleproc 和 initproc。

- idleproc 是第一个内核线程，代表空闲状态下的 CPU 运行。它的 pid 被设置为 0，状态为 PROC_RUNNABLE，具有一个指向内核栈的指针 kstack，标志 need_resched 被设置为 1，表示需要进行调度。此外，它的内核栈指向 bootstack，并被命名为 "idle"，用于执行cpu_idle函数。在初始化时，完成新的内核线程创建后进入死循环，用于调度其他进程线程。
- initproc 是第二个内核线程，通过 kernel_thread 函数创建，执行 init_main 函数。它的 pid 被设置为 1，状态为 PROC_UNINIT（在创建后需要被调度为可运行状态），具有一个指向内核栈的指针 kstack，标志 need_resched 被设置为 1，表示需要进行调度。它的内核栈通过 kernel_thread 函数创建，被命名为 "init"。在 proc_init 函数中，首先初始化了进程链表 proc_list 和哈希表 hash_list。然后通过 alloc_proc 函数分配了 idleproc 的进程结构，并检查了该结构的一些字段的正确性。接下来，设置了 idleproc 的一些属性，如 pid、状态、内核栈等。最后，通过 kernel_thread 函数创建了 initproc，并将其添加到进程链表中。该线程用于调用打印helloworld的线程。

## 扩展练习 Challenge：

- 说明语句 `local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？

在练习三中proc_run函数的完善中就使用到了这两个语句。

```c++
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {
        bool intr_flag;
        local_intr_save(intr_flag);
	······
	······
        local_intr_restore(intr_flag);
    }
}

```

其中的 `local_intr_save(intr_flag);....local_intr_restore(intr_flag);`在krn/sync/sync.h中定义如下：

```c++
//kern/sync/sync.h
#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        //read_csr(sstatus)读取控制寄存器sstatus位与操作检查其中的SIE位
        //如果SIE位为1，表示中断允许
        intr_disable();//关闭中断
        return 1;//保存中断状态
    }
    return 0;//中断禁止，中断状态未被保存
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();//开启中断
    }
}

///kern/driver/intr.c
/* intr_enable - enable irq interrupt */
void intr_enable(void) { 
    //将SSTATUS寄存器的SIE位设置为1，以允许中断触发和响应。
    set_csr(sstatus, SSTATUS_SIE); 
}

/* intr_disable - disable irq interrupt */
void intr_disable(void) { 
    //将SSTATUS寄存器的SIE位清除为0，以禁止中断触发和响应。
    clear_csr(sstatus, SSTATUS_SIE); 
}


```

local_intr_save(intr_flag);....local_intr_restore(intr_flag);语句块用于保存和恢复中断状态。

- local_intr_save(x)宏定义中，__intr_save()函数被调用来保存当前的中断状态，并将其赋值给变量x。

  - __intr_save函数首先通过read_csr(sstatus)读取控制寄存器sstatus的值，并使用位与操作检查其中的SIE位（Supervisor Interrupt Enable）。
  - 如果SIE位为1，表示中断允许，则调用intr_disable()函数关闭中断，并返回1，表示中断状态已被保存；
    - intr_enable函数通过调用set_csr函数，将SSTATUS寄存器的SIE位设置为1，以允许中断触发和响应。
  - 如果SIE位为0，表示中断禁止，则直接返回0，表示中断状态未被保存。
- local_intr_restore(x)宏定义中，__intr_restore(x)函数被调用来恢复之前保存的中断状态。即将变量x的值作为参数传递给__intr_restore()函数，以恢复之前保存的中断状态。

  - __intr_restore函数接受一个布尔类型的参数flag，如果flag为真，则调用intr_enable()函数开启中断；否则不执行任何操作。
    - intr_disable函数通过调用clear_csr函数，将SSTATUS寄存器的SIE位清除为0，以禁止中断触发和响应。

