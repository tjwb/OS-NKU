# Lab1实验报告

## 练习一：理解内核启动中的程序入口操作

### 题目

阅读`kern/init/entry.S`内容代码，结合操作系统内核启动流程，说明指令`la sp, bootstacktop`完成了什么操作，目的是什么？`tail kern_init`完成了什么操作，目的是什么？

### 解答

指令 la 是 RISC-V 的伪指令，用于将标签 bootstacktop 代表的内存地址加载到寄存器中。在这个指令中，它将 bootstacktop 的地址加载到栈指针寄存器 sp 中。栈指针 sp（Stack Pointer）用来指向当前调用栈的顶部，通过将 bootstacktop 的地址加载到 sp，栈指针被初始化，指向内核栈的顶部。这样做的目的是设置内核栈地址，将栈指针指向栈顶，让操作系统在内核启动过程中使用的栈空间被正确初始化。

tail伪指令是RISC-V中的尾调用优化指令，用于无条件跳转到一个目标函数并且不会返回，这种跳转方式不会为新的函数调用分配额外的栈空间，因此 kern_entry 的栈帧会被释放。而kern_init是目标函数标签，此处使用tail伪指令可以使程序跳转到名为kern_init的函数。这样做的目的是通过进入初始化函数完成初始化操作。同时tail的这种尾调用方式可以避免在启动过程中消耗不必要的栈空间，从而使内核的启动更加高效。

## 练习二：完善中断处理 （需要编程）

### 题目

请编程完善`trap.c`中的中断处理函数`trap`，在对时钟中断进行处理的部分填写`kern/trap/trap.c`函数中处理时钟中断的部分，使操作系统每遇到100次时钟中断后，调用`print_ticks`子程序，向屏幕上打印一行文字”100 ticks”，在打印完10行后调用`sbi.h`中的`shut_down()`函数关机。

要求完成问题1提出的相关函数实现，提交改进后的源代码包（可以编译执行），并在实验报告中简要说明实现过程和定时器中断中断处理的流程。实现要求的部分代码后，运行整个系统，大约每1秒会输出一次”100 ticks”，输出10行。

### 解答
```c
case IRQ_S_TIMER:
            // "All bits besides SSIP and USIP in the sip register are
            // read-only." -- privileged spec1.9.1, 4.1.4, p59
            // In fact, Call sbi_set_timer will clear STIP, or you can clear it
            // directly.
            // cprintf("Supervisor timer interrupt\n");
             /* LAB1 EXERCISE2   YOUR CODE : 2212495 2210386 1913697 */
            clock_set_next_event();  
    	    ticks++;  
    	    if (ticks%100 == 0) {
        	cprintf("100 ticks\n");
            num++;
    	    }
    
    	    if (num == 10) {
        	sbi_shutdown();  
	    }
            break;
```
运行结果如下：
![图片1](https://github.com/liuwugou/image_for_os/blob/main/lab1_1.png)

## 扩展练习Challenge1：描述与理解中断流程

### 题目

描述 ucore 中处理中断异常的流程（从异常的产生开始），其中 mov a0，sp 的目的是什么？SAVE_ALL中寄存器保存在栈中的位置是什么确定的？对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。

### 中断处理流程

在`trapentry.S`中，处理中断和异常的流程如下：

1. 异常产生：
   - 中断异常是由于某种事件（例如外部硬件触发的事件，例如时钟中断或外部设备输入）而引发的。当这样的事件发生时，处理器会产生一个异常。

2. 进入异常处理程序：
   - 异常首先引发处理器进入异常处理程序。在这段代码中，异常处理程序的入口点是 `__alltraps` 标签。

3. 保存当前状态：
   - 异常处理程序开始时，首先调用 `SAVE_ALL` 宏，它的目的是保存当前的处理器状态。这包括保存通用寄存器（`x0`到`x31`）的值、状态寄存器（`s0`到`s4`）的值以及栈指针（`sp`）的值。（其中，四个寄存器的作用分别为：`sstatus`——保存当前CPU的状态；`sepc`——保存触发中断or异常的指令地址；`sbadaddr`——出现一些地址相关异常时的异常地址；`scause`——存储异常or中断发生的原因 是一个错误代码。）

4. 调用异常处理函数：
   - 接下来，通过 `jal trap` 指令调`trap` 的异常处理函数。这个函数将会处理特定类型的异常，根据异常类型执行相应的操作。在 `trap` 函数中，可以访问保存的寄存器状态以及特殊系统寄存器，以便进行异常处理。

5. 处理异常：
   - 在 `trap` 函数中，针对中断异常，可以执行必要的操作。这可能包括处理中断请求，保存当前执行上下文，执行中断处理程序，或者根据中断类型执行其他操作。处理完成后，`trap` 函数可能会更新寄存器状态，以便在返回时将控制权恢复到正确的位置。

6. 恢复之前的状态：
   - 异常处理程序在处理完异常后，通过 `RESTORE_ALL` 宏恢复之前保存的处理器状态。这个过程包括恢复通用寄存器的值、特殊系统寄存器的值以及栈指针的值。但是，值得注意的是，`restore`部分中仅仅恢复了`sstatus`和`sepc`两个寄存器的值，对于第三个和第四个并没有恢复。这是因为当处理完中断过后，意味着该问题已经被解决，就不需要保存这部分的内容以及恢复这一部分的内容了。

7. 返回到用户模式：
   - 最后，通过 `sret` 指令，处理器返回到用户模式执行。这将控制权返回给之前被中断的程序，使其能够继续执行。
  
### `mov a0, sp`指令含义

`move a0, sp` 这条指令为了将当前的堆栈指针 (`sp`) 的值传递给 `trap` 函数作为参数。在 RISC-V 架构中，函数参数通常是通过一些特定的寄存器来传递的，其中 `a0` 到 `a6` 寄存器被用作函数参数寄存器。因此，将当前堆栈指针的值存储在 `a0` 寄存器中，实际上是为了在调用 `trap` 函数时将堆栈指针的值传递给该函数，以便该函数能够知道中断发生时的堆栈位置。异常处理函数通常需要访问中断发生时的上下文信息，例如堆栈状态，以便能够正确地处理异常情况。通过将堆栈指针传递给 `trap` 函数，可以确保在异常处理期间能够访问当前的堆栈状态。

### SAVE_ALL寄存器保存在栈中的位置确定

在 `SAVE_ALL` 宏中，寄存器的保存位置是通过以下方式确定的：

1. 宏开始时，将当前栈指针 (`sp`) 的值保存到 `sscratch` 寄存器中。这是为了在异常处理期间保存原始栈指针的值，以便在恢复时使用。

2. 然后，通过逐一保存通用寄存器（`x0` 到 `x31`）的值，将它们依次存储在栈上。每个寄存器都按照其编号乘以 `REGBYTES` 的偏移量来确定存储位置。例如，`x0` 存储在 `0*REGBYTES(sp)` 处，`x1` 存储在 `1*REGBYTES(sp)` 处，以此类推。这个偏移量乘以 `REGBYTES` 是为了确保每个寄存器之间的存储位置之间有足够的空间。

3. 接着，特殊的系统寄存器（`s0` 到 `s4`）的值也被存储在栈上，其位置也是通过乘以 `REGBYTES` 的方式来确定的。

综上所述，每个寄存器的保存位置都是根据其在通用寄存器集（`x0` 到 `x31`）和系统寄存器集（`s0` 到`s4`）中的编号来确定的，并且这些位置都相对于栈指针 (`sp`) 的当前值进行计算。这确保了在保存和恢复寄存器时，它们的值被正确地存储和读取，并且不会互相覆盖。

### __alltraps 的寄存器保存问题

`__alltraps` 需要保存所有寄存器，其目的是为了处理各种类型的异常，以确保在异常处理程序中能够访问所有的寄存器状态。

## Challenge2：理解上下文切换机制

在 `trapentry.S` 中汇编代码 `csrw sscratch, sp；csrrw s0, sscratch, x0` 实现了什么操作，目的是什么？`save all`里面保存了 `stval scause` 这些 `csr`，而在 `restore all` 里面却不还原它们？那这样 `store` 的意义何在呢？

### `csrw sscratch, sp；csrrw s0, sscratch, x0` 的功能

csrw指令用于在RISC-V架构中对控制和状态寄存器进行写入操作。例如，指令`csrw sstatus, s1`表示将寄存器`s1`的值写入到状态寄存器`sstatus`中，类似于状态寄存器中的`move`操作。而指令`csrw sscratch, sp`将当前堆栈指针`sp`的值存储到`sscratch`寄存器中。需要注意的是，`sscratch`寄存器通常被用作临时寄存器，其主要目的是在中断发生时，为了保存内核的堆栈指针而创建的一个临时存储空间。

指令`csrrw s0, sscratch, x0`表示读取`sscratch`寄存器的值，并将其写入到寄存器`s0`中，同时将寄存器`x0`的值写入原本的`sscratch`寄存器中。由于寄存器`x0`通常被称为零寄存器，因此这条指令的效果是将原本`sscratch`寄存器的值存储到寄存器`x0`中，并将`sscratch`寄存器的值设置为零。

如果在这之后再次发生异常或中断，`sscratch`寄存器的值为零，而原本的`sscratch`寄存器的值存储在寄存器`s0`中，没有丢失信息。如果出现递归中断或异常情况，通过检查`sscratch`寄存器的值，可以确定是否来源于内核。在`csrr`指令之后，后续的`STORE`指令将保存在寄存器`s0`到`s4`中的值存储在内存中，以确保在异常处理过程中这些值不会被覆盖，并且可以在处理完成后进行恢复。

###  `restore all` 中的寄存器维护问题

在上述代码的`RESTORE_ALL`部分，只有`sstatus`和`sepc`两个寄存器的值被恢复，而`scause`和`sbadaddr`的值没有被还原。这是因为`scause`寄存器存储了异常发生的原因，而`sbadaddr`寄存器存储了某些异常相关的地址。一旦异常或中断处理完成，就不再需要保留这些信息了。所以，这段代码选择只恢复必要的寄存器，而忽略了`scause`和`sbadaddr`寄存器的值。这样可以有效地管理寄存器状态，并确保异常处理后程序的继续执行。

## Challenge3：完善异常中断

编程完善在触发一条非法指令异常 mret 和，在 kern/trap/trap.c 的异常处理函数中捕获，并对其进行处理，简单输出异常类型和异常指令触发地址，即“Illegal instruction caught at 0x(地址)”，“ebreak caught at 0x（地址）”与“Exception type:Illegal instruction”，“Exception type: breakpoint”。

### 解答

```c
case CAUSE_ILLEGAL_INSTRUCTION:
             // 非法指令异常处理
             /* LAB1 CHALLENGE3   YOUR CODE :2212495 2210386 1913697  */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Illegal instruction\n");
            cprintf("%x",tf->epc);
            tf->epc+=4;
            break;

case CAUSE_BREAKPOINT:
            //断点异常处理
            /* LAB1 CHALLLENGE3   YOUR CODE :2212495 2210386 1913697  */
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("breakpoint\n");
            cprintf("%x",tf->epc);
            tf->epc+=2;
            //断点指令比较短，只需要+2
            break;
```
运行结果如下：
![图片2](https://github.com/liuwugou/image_for_os/blob/main/lab1_2.png)