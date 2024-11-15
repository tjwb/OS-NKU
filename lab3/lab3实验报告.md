# lab3实验报告
## 实验要求
- 基于markdown格式来完成，以文本方式为主
- 填写各个基本练习中要求完成的报告内容
- 列出你认为本实验中重要的知识点，以及与对应的OS原理中的知识点，并简要说明你对二者的含义，关系，差异等方面的理解（也可能出现实验中的知识点没有对应的原理知识点）
- 列出你认为OS原理中很重要，但在实验中没有对应上的知识点
  
## 练习1：理解基于FIFO的页面替换算法（思考题）
### 换入换出流程
首先，当发⽣缺⻚异常时，出现异常的地址会传⼊ do_pgfault 函数进⾏缺⻚的处理。首先调用find_vma()尝试找到包含了发生错误的虚拟地址的vma，对异常情况进行判断，确定是缺页造成的访问异常还是非法的虚拟地址访问造成的，然后将pgfault_num计数加一，更新PTE的权限标志位，使用ROUNDDOWN()取得虚拟地址对应的页首地址。尝试使用get_pte找到或创建这个地址对应的二级页表项。接下来会使⽤
get_pte 函数获取/创建对应虚拟地址映射过程中的⻚表和⻚表项：
- 若该页表项中每一位都是0，表明其原本确实不存在，则需要为其分配一个初始化后全新的物理页， 并建立虚实映射关系。
  - 调用pgdir_alloc_page()，首先调用宏定义的alloc_page()（即alloc_pages(1)），向页内存管理器尝试申请一个页的内存，失败则调用swap_out()进行页的换出，最终得到一个空闲的物理页用于建立映射，成功建立后调用swap_map_swappable()设置映射的这个物理页为可交换，并设置关联的虚拟地址，检查引用次数是否为1。
- 若得到的⻚表项不为 0，表⽰这个地址对应的⻚是在之前被换出的。此时需要调⽤ swap_in 将⻚⾯内容从“硬盘”中通过 swapfs_read 读⼊对应的⻚⾯，并且使⽤ page_insert 将虚拟地址和物理⻚⾯的映射关系写⼊对应的⻚表项中。之后再使⽤ swap_map_swappable 调⽤不同策略的 map_swappable
进⾏维护，从⽽保证⻚⾯置换能够正确执⾏。
### 用到的函数
1. do_pgfault()
访问页面缺失时，最先进入该函数进行处理。
2. find_vma()
判断访问出错的虚拟地址是否在该页表的**合法虚拟地址集合（所有可用的虚拟地址/虚拟页的集合，不论当前这个虚拟地址对应的页在内存上还是在硬盘上）里。
3. get_pte()
若合法，则获取该va对应的页表项。其函数中包含了两种处理即查找和分配的合并，即其会依次判断，二级页目录项，一级页目录项，页表项是否存在，若不存在则分配新的项给这些部分。
4. set_page_ref()
将页 page的引用次数设置为 val。
5. swap_out_victim()
本过程中调用的是swap_fifo.c中的 _fifo_swap_out_victim()，把FIFO队列尾端的元素删除，并将删除的页的地址赋值给 ptr_page。
6. alloc_page / alloc_pages
前者为后者的⼀个宏，会分配⼀个⻚⾯，如果不能分配则使⽤ swap_out 换出所需的⻚⾯
数量。
7. swap_in()
这个函数会调用 swapfs_read()从磁盘中读入 addr对应的物理页的数据，写入声明分配得到的一个新物理页中，将 ptr_result指向这个物理页。其中，assert(result!=NULL)：确保分配到了一个新的物理页。assert(r!=0)：确保从磁盘中读取到了有效数据。
8. page_insert()
将虚拟地址和新分配的⻚⾯的物理地址在⻚表内建⽴⼀个映射。
9. swap_out()
页面需要换出时调用该函数。换出时机策略是消极策略，只有内存空闲页不够时才会换出。
10. swap_map_swappable()
换入页面之后调用，把换入的页面加入到FIFO的交换页队列中。

## 练习2：深⼊理解不同分⻚模式的⼯作原理

### 解释两段代码为什么如此相像

RISC-V 中⽬前共有四种分⻚模式 Sv32，Sv39，Sv48，Sv57。其中 Sv32 使⽤了⼆级⻚表，Sv39 使⽤了三级⻚表，Sv48 和 Sv57 则
是使⽤了四级和五级⻚表。在我们的ucore实验中使⽤的是Sv39模式，⻚表被分为了：PDX1、PDX0和PTX。

get_pte 的⽬的是查找/创建特定线性地址对应的⻚表项。由于 ucore 使⽤ Sv39 的分⻚机制，所以共使⽤了三级⻚表，
get_pte 中的两段相似的代码分别对应于在第三、第⼆级⻚表（PDX1，PDX0）的查找/创建过程：

- ⾸先 get_pte 函数接收⼀个参数 create 标识未找到时是否要分配新⻚表项。之后⾸先通过 PDX1 宏获取线性地址 la
对应的第⼀级 PDX1 的值，并且在 pgdir 即根⻚表（⻚⽬录）中找到对应的地址：

```
// 找到对应的Giga Page
pde_t *pdep1 = &pgdir[PDX1(la)];
```

并且判断其 V 标志位是否为真（表⽰可⽤）。若可⽤则继续寻找下⼀级⽬录，若不可⽤则根据 create 的值决定是否调⽤
alloc_page() 开辟⼀个新的下⼀级⻚表，之后设置这⼀级⻚表的⻚表项，⻚⾯的引⽤以及下⼀级⻚表的⻚号的对应关系。

- 之后再对下⼀级⻚表进⾏同样的操作，其中通过 PDE_ADDR 得到⻚表项对应的物理地址。
- 最后根据得到的 pdep0 的⻚表项找到最低⼀级⻚表项的内容并且返回。

因此可以看出，这两段代码如此相像的原因为：由于sv39为三级⻚表，get_pte函数的⽬标是从虚拟地址找到pte的地址，所以⾸先
会使⽤pgdir（指向PDX1）找到PDX0的地址，然后再通过PDX0找到PTX的地址，由于⻚表的索引特性，这就导致了这两段代码的
结构⾮常相似。最后通过PTX(la)索引就得到了pte的地址。
整个 get_pte 会对 Sv39 中的⾼两级⻚⽬录进⾏查找/创建以及初始化的操作，并且返回对应的最低⼀级⻚表项的内容。两段相似
的代码分别对应了对不同级别 PDX（PDE）或 PTX （PTE）的操作。

### 查找和分配的功能

我们小组认为这两个功能合并在一起是好的。
因为目前阶段实现的操作系统内容相对较少，功能相对单薄。使用get_pte()函数进行页表项的查找和分配时涉及的代码量较少且结构清晰，逻辑流程简单。
将上述功能合并在一个函数内不会造成代码的易读性下降，相反还能减少函数的调用，简化代码，更加适合学习。
此外较少的函数调用可以减少系统开销，优化性能。

## 练习3：给未被映射的地址映射上物理页
首先是补充的代码部分：
```
if (swap_init_ok)
        {
            struct Page *page = NULL;
            // 你要编写的内容在这里，请基于上文说明以及下文的英文注释完成代码编写
            //(1)According to the mm AND addr, try
            // to load the content of right disk page
            // into the memory which page managed.
            //(2) According to the mm,
            // addr AND page, setup the
            // map of phy addr <--->
            // logical addr
            //(3) make the page swappable.

            swap_in(mm, addr, &page);
            page_insert(mm->pgdir, page, addr, perm);
            swap_map_swappable(mm, addr, page, 1);

            page->pra_vaddr = addr;
        }
```
上述代码的执行流程如下：
1. ⾸先我们需要将⼀个磁盘⻚调到内存中，调⽤ swap_in() 函数 ，⽤ mm_struct *mm 和 addr 作为参数调⽤ get_pte 查找/
构建该虚拟地址对应的⻚表项，并通过 alloc_page() 分配物理⻚和 swapfs_read() （本质上是memcpy）将数据从硬盘
读到内存⻚result中，&page利⽤传参完成赋值，保存换⼊的物理⻚⾯。
2. 我们需要将换⼊的物理⻚添加虚拟⻚映射，调⽤ page_insert 函数更新⻚表/插⼊新的⻚表项并刷新TLB。在
page_insert 中还是先找该虚拟地址对应的⻚表项，如果valid（说明这地⽅有个PTE）则检查该⻚是否和要插⼊的⻚相同；
最后调⽤ *ptep = pte_create(page2ppn(page), PTE_V | perm) 建⽴该⻚对应的PTE。
3. 设置该⻚⾯可交换：找到swap.c中的 swap_map_swappable 函数，设置好对应参数即可调⽤。

### Q1:请描述⻚⽬录项（Page Directory Entry）和⻚表项（Page Table Entry）中组成部分对ucore实现⻚替换算法的潜在⽤处。
对于改进的时钟（Enhanced Clock）页替换算法，标志位PTE_A 可以表示页面是否被访问，PTE_D 可以表示页面是否被修改。
根据这两个标志，页面有四种状态：
(0, 0)：未被访问且未修改，优先淘汰；
(0, 1)：未被访问但已修改，次选淘汰；
(1, 0)：已访问但未修改，再次选择；
(1, 1)：已访问且已修改，最后淘汰。
这种算法能够优先淘汰未修改的页面，减少磁盘 I/O 操作。然而，为了找到合适的页面，可能需要多次扫描，增加了执行开销。

### 如果ucore的缺页服务例程在执行过程中访问内存，出现了页访问异常，请问硬件要做哪些事情？

缺⻚异常是指CPU访问的虚拟地址时， MMU没有办法找到对应的物理地址映射关系，或者与该物理⻚的访问权不⼀致⽽发⽣的异
常。在 kern_init 中初始化物理内存后调⽤ idt_init 初始化中断描述表，将寄存器 sscratch 赋值为0表⽰现在执⾏的是内核代码；将
异常向量地址 stvec 设置为 &__alltraps 并将 sstatus 设置为内核可访问⽤⼾内存。
出现⻚访问异常时，trapframe 结构体中的相关寄存器被修改，按照⼀般异常处理的流程，产⽣异常的指
令地址会存⼊ sepc 寄存器，访问的地址存⼊ stval ，并且根据设置的 stvec 进⼊操作系统的异常处理过程。于是
kern/trap/trap.c 的 exception_handler() 被调⽤，根据 trapframe *tf->cause（也就是scause）的值
CAUSE_LOAD_PAGE_FAULT /CAUSE_STORE_PAGE_FAULT调⽤ pgfault_handler，其会调用do_pgfalut进⾏进⼀步处理。

### 数据结构Page的全局变量（其实是一个数组）的每一项与页表中的页目录项和页表项有无对应关系？如果有，其对应关系是啥？

sv39 分⻚机制下，每⼀个⻚表所占⽤的空间刚好为⼀个⻚的⼤⼩。在处理缺⻚时，如果⼀个虚拟地址对应的⼆级、三级⻚表项（⻚
⽬录项）不存在，则会为其分配⼀个⻚，当第⼀级⻚表项没有设置过时也会分配⼀个⻚（主要体现在上⽂的 get_pte 中）。此
外，当⼀个⻚⾯被换出时，他所对应的⻚⾯会被释放，当⼀个⻚⾯被换⼊或者新建时，会分配⼀个⻚⾯。所以，对于本实验中缺⻚
机制所处理和分配的所有⻚⽬录项、⻚表项，都对应于 pages 数组中的⼀个⻚，但是 pages 中的⻚并不⼀定会全部使⽤。

## 练习4：补充完成Clock页替换算法
首先是_clock_init_mm()函数：
```
_clock_init_mm(struct mm_struct *mm)
{
    
     /*LAB3 EXERCISE 4: 2212495姚明辰 1913697唐俊杰 2210386吴邦宸*/
    // 初始化pra_list_head为空链表
    // 初始化当前指针curr_ptr指向pra_list_head，表示当前页面替换位置为链表头
    // 将mm的私有成员指针指向pra_list_head，用于后续的页面替换算法操作
    list_init(&pra_list_head);
    curr_ptr = &pra_list_head;
    mm->sm_priv = &pra_list_head;
    cprintf(" mm->sm_priv %x in fifo_init_mm\n", mm->sm_priv);
    return 0;
}
```

接着是_clock_map_swappable()函数：
```
_clock_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && curr_ptr != NULL);
    // record the page access situlation
    /*LAB3 EXERCISE 4:2212495姚明辰 1913697唐俊杰 2210386吴邦宸*/
    // link the most recent arrival page at the back of the pra_list_head qeueue.
    // 将页面page插入到页面链表pra_list_head的末尾
    // 将页面的visited标志置为1，表示该页面已被访问

    list_entry_t *head = mm->sm_priv;
    list_add(head, entry);
    page->visited = 1;
    curr_ptr = entry;

    return 0;
}

```
然后是_clock_swap_out_victim()函数
```
_clock_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    /* Select the victim */
    //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
    //(2)  set the addr of addr of this page to ptr_page
    list_entry_t *tmp = head;
    while (1)
    {
        /*LAB3 EXERCISE 4: 2212495姚明辰 1913697唐俊杰 2210386吴邦宸*/
        // 编写代码
        // 遍历页面链表pra_list_head，查找最早未被访问的页面
        // 获取当前页面对应的Page结构指针
        // 如果当前页面未被访问，则将该页面从页面链表中删除，
        并将该页面指针赋值给ptr_page作为换出页面
        // 如果当前页面已被访问，则将visited标志置为0，表示该页面已被重新访问

        list_entry_t *entry = list_prev(tmp);
        struct Page *p = le2page(entry, pra_page_link);

        if (p->visited == 0)
        {
            list_del(entry);
            *ptr_page = p;
            cprintf("curr_ptr %p\n", curr_ptr);
            break;
        }

        if (p->visited == 1)
        {
            p->visited = 0;
        }

        tmp = entry;
    }
    return 0;
}
```
Clock页替换算法的实现思路和FIFO算法的不同之处如下：

在Clock页替换算法中，页面链表pra_list_head中，每当一个新页面加入时，它会被添加到链表的末尾；而在执行页面换出时，
算法从一个全局指针curr_ptr开始查找，寻找第一个未被访问的页面。_clock_swap_out_victim函数中的循环正是用于从curr_ptr开始查找的。
如果一个页面已经被访问过，算法会将其visited状态重置为0（即未访问）。如果页面未被访问，则会通过list_del函数将该页面从链表中删除，
并将该页面的指针赋给参数ptr_page（*ptr_page = ptr），作为被换出的页面。如果链表中所有页面都被访问过，算法会按照该逻辑遍历一圈，最终删除curr_ptr指向的页面。
其中，curr_ptr在Clock算法中指向的是最旧的页面，并且会随着链表的插入和删除发生变化，而不是每次都从链表头部开始遍历。

与Clock算法不同，FIFO算法每次加入新页面时，页面被添加到链表的头部，而在进行页面换出时，算法通过list_prev来找到链表头部的前一个节点
（由于是双向链表，直接可以找到尾部节点），并将该页面作为换出的页面。如果当前页面不是链表头部（即链表头部只有一个元素时，按照换出逻辑不会出现这种情况），
则会将该页面赋值给ptr_page作为换出的页面。

综合来看，Clock算法考虑了页面的访问状态，而不是像FIFO那样简单粗暴地替换最早进入的页面。

## 练习5：阅读代码和实现手册，理解页表映射方式相关知识(如果我们采用”一个大页“ 的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？)
好处：
1. 连续内存分配：大页可以为需要大量连续内存的应用程序提供更好的性能，因为它们减少了页表条目的数量和TLB缺失的可能性。
2. 简单性：使用一个大页的页表映射方式更为简单和直观。
3. 减少TLB缺失：由于大页涵盖的物理内存范围更大，TLB中的一个条目可以映射更大的内存范围，从而可能减少TLB缺失的次数。
4. 快速访问：由于只有一个页表，页表查找速度通常更快，从而可以减少内存访问的延迟。
   

坏处：
1. 在多个进程中使用大页映射（如 Sv39 下的 1GiB 页面）时，发生缺页异常和页面置换时需要将整个大页的内容交换到硬盘，换回时也需全部写回。
当物理内存不足且进程较多时，这会导致程序运行速度下降。
1. 还可能会导致内存碎⽚的问题，内存利⽤率较低。
2. 安全隐患问题：⼀个⼤⻚泄露更易导致更多信息泄露。
3. 兼容性问题：不是所有的硬件和操作系统都支持大页。
4. 不灵活：大页不适合小内存需求的应用程序。

## 扩展练习 Challenge：实现不考虑实现开销和效率的LRU页替换算法

LRU算法每次剔除的是最久未使用的页面，所以在每次插入页面时首先进行一次查询，查看是否原链表中有相同的页面，如果有则删除该页面，并将该页面插入至首元素，
表示最近被访问的页面（把该页面替换为链表头页面）；如果没有则直接插入到头节点处；
而在删除页面时，只需要删除链表尾页面即可（因为每次使用页面都会重置顺序，所有链表尾部的节点就是最久没有被使用的）。

首先是初始化链表的代码，这一部分和fifo以及clock类似。
```
_lru_init_mm(struct mm_struct *mm)
{

    list_init(&pra_list_head);
    curr_ptr = &pra_list_head;
    mm->sm_priv = &pra_list_head;
    cprintf(" mm->sm_priv %x in fifo_init_mm\n", mm->sm_priv);
    return 0;
}

```

接下来是处理页面插入的函数，若页面原本不在链表中,
将新访问的页面添加到链表头部，表示该页面是最近刚被访问过的. 若页面原本已在链表中，
则先将其从原来位置删除后再插入到链表头，以更新页面的访问顺序信息.

```
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && curr_ptr != NULL);
    list_entry_t *head = mm->sm_priv;

    // 若页面原本不在链表中, 将新访问的页面添加到链表头部，表示该页面是最近刚被访问过的. 若页面原本已在链表中，
    则先将其从原来位置删除后再插入到链表头，以更新页面的访问顺序信息.
    list_entry_t *is_page_already_in = get_list_page(head, page);
    if (is_page_already_in)
    {
        list_del(is_page_already_in);
    }

    list_add(head, entry);
    curr_ptr = entry;

    return 0;
}

```

在这里定义了一个辅助函数get_list_page，用来判断要访问的页面是否在链表内，如果在就返回该页面，不在就返回null。

```
list_entry_t *get_list_page(list_entry_t *head, struct Page *target_page)
{
    list_entry_t *entry;
    for (entry = (head)->next; entry != (entry); entry = entry->next)
    {
        struct Page *page = le2page(entry, pra_page_link);
        if (page == target_page)
        {
            return entry;
        }
    }
    return NULL;
}

```

然后是换出页面的函数，只需删除链表尾部的节点即可。

```
_lru_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);

    list_entry_t *le = head->prev;

    curr_ptr = le;

    struct Page *curr_page = le2page(curr_ptr, pra_page_link);

    list_del(le);

    cprintf("out:curr_ptr %p\n\n", curr_ptr);
    assert(curr_ptr != NULL);

    *ptr_page = curr_page;
    return 0;
}

```

最后是运行结果

![lab3_challenge](https://github.com/liuwugou/image_for_os/blob/main/lab3_challenge.png)
