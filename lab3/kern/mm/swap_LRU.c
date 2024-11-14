#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_LRU.h>
#include <list.h>

static list_entry_t pra_list_head;
static list_entry_t *curr_ptr;

// 初始化
static int
_lru_init_mm(struct mm_struct *mm)
{

    list_init(&pra_list_head);
    curr_ptr = &pra_list_head;
    mm->sm_priv = &pra_list_head;
    cprintf(" mm->sm_priv %x in fifo_init_mm\n", mm->sm_priv);
    return 0;
}

// 判断目标页面是否在链表中，找到目标页面时返回对应的链表项指针。
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

// 插入
static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *entry = &(page->pra_page_link);

    assert(entry != NULL && curr_ptr != NULL);
    list_entry_t *head = mm->sm_priv;

    // 若页面原本不在链表中, 将新访问的页面添加到链表头部，表示该页面是最近刚被访问过的. 若页面原本已在链表中，则先将其从原来位置删除后再插入到链表头，以更新页面的访问顺序信息.
    list_entry_t *is_page_already_in = get_list_page(head, page);
    if (is_page_already_in)
    {
        list_del(is_page_already_in);
    }

    list_add(head, entry);
    curr_ptr = entry;

    return 0;
}

// 删除页面——删除链表尾的页面
static int
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

// 测试代码
static int _lru_check_swap(void)
{
#ifdef ucore_test
    int score = 0, totalscore = 5;
    cprintf("%d\n", &score);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
    ++score;
    cprintf("grading %d/%d points", score, totalscore);
#else
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 4);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 4);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 4);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 4);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 5);
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num == 5);
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num == 5);
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num == 5);
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num == 5);
    assert(*(unsigned char *)0x1000 == 0x0a);
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num == 6);
#endif
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
    return 0;
}
// 时钟中断的时候什么都不做

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