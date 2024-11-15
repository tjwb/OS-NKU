#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_LRU.h>
#include <list.h>

static list_entry_t pra_list_head;

// 初始化
static int
_lru_init_mm(struct mm_struct *mm)
{
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    // cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
    return 0;
}

// 插入
static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && head != NULL);
    // record the page access situlation

    //(1)link the most recent arrival page at the back of the pra_list_head qeueue.
    list_add(head, entry);
    return 0;
}

// 删除页面——删除链表尾的页面
static int
_lru_swap_out_victim(struct mm_struct *mm, struct Page **ptr_page, int in_tick)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL);
    assert(in_tick == 0);
    /* Select the victim */
    //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
    //(2)  set the addr of addr of this page to ptr_page
    list_entry_t *entry = list_prev(head);
    if (entry != head)
    {
        list_del(entry);
        *ptr_page = le2page(entry, pra_page_link);
    }
    else
    {
        *ptr_page = NULL;
    }
    return 0;
}

// 测试代码
static int _lru_check_swap(void)
{
    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    cprintf("write Virt Page a in lru_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4); // 来自fifo算法的两次测试； 链表加载为-d-c-b-a形式，以及pgfault成为4的过程在swap.c的初步测试中

    cprintf("页面链表形式：head-d-c-b-a，pte全为1\n");

    cprintf("write Virt Page e in lru_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5); // 此处换出逻辑应于fifo相同，换出了a,并连入e

    cprintf("页面链表形式：head-e-d-c-b,pte全为1\n");
    cprintf("模拟一次时钟中断\n");
    swap_tick_event(check_mm_struct);
    cprintf("页面链表形式：head-e-d-c-b,pte全为0\n");

    cprintf("write Virt Page a in lru_check_swap\n"); //
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6); // 此处换出逻辑应于fifo相同，换出了b,并连入a
    cprintf("页面链表形式：head-a-e-d-c,pte只有a为1\n");

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 6); // 此处不换出,但是c的pte成为1
    cprintf("页面链表形式：head-a-e-d-c,pte是a和c为1\n");
    // 此处如果直接发生换出,应该换出c;但c被访问过，pte为1
    // 模拟一次时钟中断可以使所有pte为1的页连到最前

    cprintf("模拟一次时钟中断\n");
    swap_tick_event(check_mm_struct);
    cprintf("页面链表形式：head-c-a-e-d,pte全0\n");

    cprintf("write Virt Page b in lru_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 7); // 此处换出的应为d而非c,继续测试c是不是还在链表中。
    cprintf("页面链表形式：head-b-c-a-e,pte只有b为1\n");

    cprintf("write Virt Page c in lru_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 7); // 此处不换出,但是c的pte成为1；同时此断言正确说明c还在链表中，算法正确。

    cprintf("write Virt Page d in lru_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 8); // 断言正确说明d在之前被换出

    cprintf("最终的页面链表形式：head-d-b-c-a,pte只有a为0\n");
    return 0;
}

static int
_lru_init(void)
{
    return 0;
}
// 初始化的时候什么都不做

static int
_lru_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct *mm)
{
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    if (head == NULL)
    {
        return 0; // 如果链表头为空直接返回
    }

    list_entry_t *cur = list_prev(head); // 从链表最后一个节点开始遍历

    while (cur != head)
    { // 当当前节点不等于链表头时，表示还没遍历完一圈
        struct Page *page = le2page(cur, pra_page_link);
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);
        list_entry_t *pre = list_prev(cur);

        if (*ptep & PTE_A)
        {
            // 删除当前节点
            list_del(cur);
            *ptep &= ~PTE_A; // 清除访问位

            // 将当前节点添加到链表头部
            list_add(head, cur); // 移动位置
        }
        cur = pre;
        // cprintf("循环一次;\n");
    }

    return 0;
}

struct swap_manager swap_manager_lru =
    {
        .name = "lru swap manager",
        .init = &_lru_init,
        .init_mm = &_lru_init_mm,
        .tick_event = &_lru_tick_event,
        .map_swappable = &_lru_map_swappable,
        .set_unswappable = &_lru_set_unswappable,
        .swap_out_victim = &_lru_swap_out_victim,
        .check_swap = &_lru_check_swap,
};