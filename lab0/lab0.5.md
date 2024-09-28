# lab0.5实验报告
## 实验内容
实验0.5主要讲解最小可执行内核和启动流程。我们的内核主要在 Qemu 模拟器上运行，它可以模拟一台 64 位 RISC-V 计算机。为了让我们的内核能够正确对接到 Qemu 模拟器上，需要了解 Qemu 模拟器的启动流程，还需要一些程序内存布局和编译流程（特别是链接）相关知识,以及通过opensbi固件来通过服务。
## 练习1: 使用GDB验证启动流程
为了熟悉使用qemu和gdb进行调试工作,使用gdb调试QEMU模拟的RISC-V计算机加电开始运行到执行应用程序的第一条指令（即跳转到0x80200000）这个阶段的执行过程，说明RISC-V硬件加电后的几条指令在哪里？完成了哪些功能？
### CPU上电复位
1.进入lab0目录下，打开终端，或者是从终端进入到lab0目录下，使用make debug命令，进入调试模式，此时该终端就停在此界面,不能执行其他命令。输出信息如下：
```
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
+ cc kern/driver/console.c
+ cc libs/printfmt.c
+ cc libs/readline.c
+ cc libs/sbi.c
+ cc libs/string.c
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

```
1.新建一个终端，同样到lab0目录下，执行make gdb命令，进入gdb调试模式。
2.首先执行的是`x/10i 0x80000000`指令，获取CPU加电以后的10条指令，输出如下：
```
=> 0x1000:	auipc	t0,0x0
   0x1004:	addi	a1,t0,32
   0x1008:	csrr	a0,mhartid
   0x100c:	ld	t0,24(t0)
   0x1010:	jr	t0
   0x1014:	unimp
   0x1016:	unimp
   0x1018:	unimp
   0x101a:	0x8000
   0x101c:	unimp
```
3.使用`si`指令单步执行程序，同时可以通过指令`info r t0`查看t0寄存器的值，得到如下结果;
```
(gdb) si
0x0000000000001004 in ?? ()
(gdb) info r t0
t0             0x1000	4096
(gdb) si
0x0000000000001008 in ?? ()
(gdb) info r t0
t0             0x1000	4096
(gdb) 
```
4.分析其中的每一条指令：
* `auipc t0,0x0`:将地址0x1000（即当前指令的地址）的偏移量（0x1000）加载到t0中。
* `addi a1,t0,32`:将t0的值加上32，得到0x1020，并将结果赋值给a1。
* ` csrr a0,mhartid`:将hartid（hartid是hart的ID号，hart是处理器内核，hartid是hart的编号）的值赋值给a0。
* `ld t0,24(t0): t0= [0x1000+24] = 0x80000000`
* `jr t0`:跳转到t0的值，即跳转到0x800000000。
* `unimp`:表示一个非法指令，这个指令用于表示一个未定义的操作，通常用于调试和测试。
### OPENSBI启动
QEMU自带的bootloader: OpenSBI固件，那么在 Qemu 开始执行任何指令之前，首先两个文件将被加载到 Qemu 的物理内存中：即作为 bootloader 的 OpenSBI.bin 被加载到物理内存以物理地址 0x80000000 开头的区域上，同时内核镜像 os.bin 被加载到以物理地址 0x80200000 开头的区域上。这里执行的就是OpenSBI.bin。
1.从上文分析可知，在CPU上电后，会执行`jr t0`指令，跳转到0x80000000，此时进入内核。使用`x/10i 0x80000000`指令，获取内核的10条指令，输出如下：
```
   0x80000000:	csrr	a6,mhartid
   0x80000004:	bgtz	a6,0x80000108
   0x80000008:	auipc	t0,0x0
   0x8000000c:	addi	t0,t0,1032
   0x80000010:	auipc	t1,0x0
   0x80000014:	addi	t1,t1,-16
   0x80000018:	sd	t1,0(t0)
   0x8000001c:	auipc	t0,0x0
   0x80000020:	addi	t0,t0,1020
   0x80000024:	ld	t0,0(t0)
```
可以看到才是t0寄存器中的值是0x80000000，也就是内核的入口。`t0             0x80000000	2147483648`
2.分析每一条指令：
```
   0x80000000:	csrr	a6,mhartid     ;a6=hartid=0
   0x80000004:	bgtz	a6,0x80000108  ;if (hartid>0) goto 0x80000108
   0x80000008:	auipc	t0,0x0      ; t0=0x80000008+0
   0x8000000c:	addi	t0,t0,1032  ;t0             0x80000410
   0x80000010:	auipc	t1,0x0    ;t1=0x80000010+0
   0x80000014:	addi	t1,t1,-16   ;t1=0x80000000
   0x80000018:	sd	t1,0(t0)     ; 
   0x8000001c:	auipc	t0,0x0   ;t0 0x8000001c
   0x80000020:	addi	t0,t0,1020  ;  t0 0x80000418
   0x80000024:	ld	t0,0(t0)     ;t0 = [0x80000418]
```
3.根据上文的描述内核镜像 os.bin 被加载到以物理地址 0x80200000 开头的区域上，上面执行的是OpenSBI.bin文件，下面就该执行os.bin了。即会跳转到/kern/init/entry.S。
4.使用`break kern_entry`在该函数这里打断点,查看接下来的指令;
```
0x80200000 <kern_entry>:	auipc	sp,0x3
   0x80200004 <kern_entry+4>:	mv	sp,sp
   0x80200008 <kern_entry+8>:	j	0x8020000a <kern_init>
   0x8020000a <kern_init>:	auipc	a0,0x3
   0x8020000e <kern_init+4>:	addi	a0,a0,-2
   0x80200012 <kern_init+8>:	auipc	a2,0x3
   0x80200016 <kern_init+12>:	addi	a2,a2,-10
   0x8020001a <kern_init+16>:	addi	sp,sp,-16
   0x8020001c <kern_init+18>:	li	a1,0
   0x8020001e <kern_init+20>:	sub	a2,a2,a0
```
可以看到程序在0x80200008处执行了跳转指令，跳转到了`.kern_init`函数。
5.执行完到断点以后，可以看到，在之前的那个终端中输出了内核的启动信息。
```
OpenSBI v0.4 (Jul  2 2019 11:53:53)
   ____                    _____ ____ _____
  / __ \                  / ____|  _ \_   _|
 | |  | |_ __   ___ _ __ | (___ | |_) || |
 | |  | | '_ \ / _ \ '_ \ \___ \|  _ < | |
 | |__| | |_) |  __/ | | |____) | |_) || |_
  \____/| .__/ \___|_| |_|_____/|____/_____|
        | |
        |_|

Platform Name          : QEMU Virt Machine
Platform HART Features : RV64ACDFIMSU
Platform Max HARTs     : 8
Current Hart           : 0
Firmware Base          : 0x80000000
Firmware Size          : 112 KB
Runtime SBI Version    : 0.1

PMP0: 0x0000000080000000-0x000000008001ffff (A)
PMP1: 0x0000000000000000-0xffffffffffffffff (A,R,W,X)
```
### 内核启动
1.在gdb调试程序中添加断点到`kern_init`函数，并使用`list`查看其内部指令，输出如下：
```
3	#include <sbi.h>
4	int kern_init(void) __attribute__((noreturn));
5	
6	int kern_init(void) {
7	    extern char edata[], end[];
8	    memset(edata, 0, end - edata);
9	
10	    const char *message = "(THU.CST) os is loading ...\n";
11	    cprintf("%s\n\n", message);
12	   while (1)
}
```
2.可以看到程序在最后使用了while(1)，所以会一直循环执行。
3.继续使用continue执行，得到下面的结果：
![alt text](image.png)
程序会一直执行，一直循环下去无法向下执行。自此内核被加载并且一直处于运行状态。只有碰到中断或者其他触发，才能停止程序。

## 实验总结
在CPU启动时，并不是CPU就获取不到指令，而是会有几条命令能够在CPU加电的时候能够获取到，从而将其引导到Bootloader。然后再到从Bootloader中，会加载内核镜像，然后跳转到内核的入口。同时这里的Bootloader就是OpenSBI。他们的地址都是确定的，不会改变的，不然计算机就无法启动，无法找到引导程序。

最后我们总结得到计算机加电以后得启动顺序为：
1.硬件初始化：
```
CPU复位。
内存初始化。
外设初始化。
```
2.加载Bootloader：
```
加载OpenSBI或其他Bootloader。
初始化内存控制器。
初始化CPU。
准备启动参数。
```
3.加载内核：
```
将内核镜像加载到内存中（如0x80200000）。
修改程序计数器（PC），跳转到内核入口点。
```

