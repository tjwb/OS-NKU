#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

free_buddy_t free_buddy;
#define free_array (free_buddy.buddy_array)
#define order (free_buddy.order)
#define nr_free (free_buddy.nr_free)
extern ppn_t first_ppn;

#define IS_POWER_OF_2(x) (!((x) & ((x) - 1)))

static uint32_t LOG2(size_t n)
{
    uint32_t power = 0;
    while (n / 2)
    {
        n = n / 2;
        power++;
    }
    return power;
}

static uint32_t POW2(size_t n)
{
    uint32_t pow = 1;
    for (int i = 0; i < n; i++)
    {
        pow *= 2;
    }
    return pow;
}
// 获取page的伙伴块
static struct Page *GET_BUDDY(struct Page *page)
{
    uint32_t power = page->property;
    size_t ppn = first_ppn + ((1 << power) ^ (page2ppn(page) - first_ppn)); // 得到伙伴块的物理页号，first_ppn为buddy system起始的物理页号
    return page + (ppn - page2ppn(page));
}

// (管理器)初始化
static void buddy_init(void)
{
    for (int i = 0; i < 16; i += 1)
    {
        list_init(free_array + i);
    }
    order = 0;
    nr_free = 0;
    return;
}
// 物理页初始化
static void buddy_init_memmap(struct Page *base, size_t real_n)
{
    assert(real_n > 0);
    // 传入的大小向下取整为2的次幂，设置取整后的值为空闲页面的大小
    struct Page *p = base;
    order = LOG2(real_n);
    size_t n = POW2(order);
    nr_free = n;
    // 对每一个页面都进行初始化
    for (; p != base + n; p += 1)
    {
        assert(PageReserved(p)); // 确保页面已保留
        p->flags = 0;            // 页面空闲
        p->property = 0;         // 页大小2^0=1
        set_page_ref(p, 0);      // 页面空闲，无引用
    }
    // 将整个空闲页加入到空闲链表数组对应项的空闲链表中
    list_add(&(free_array[order]), &(base->page_link));
    base->property = order;
    return;
}
// 分配一个内存块
static struct Page *buddy_alloc_pages(size_t real_n)
{
    assert(real_n > 0);
    if (real_n > nr_free)
        return NULL;

    struct Page *page = NULL;

    if (IS_POWER_OF_2(real_n))
    {
        order = LOG2(real_n);
    }
    else
    {
        order = LOG2(real_n) + 1;
    }

    size_t n = POW2(order);
    while (1)
    {
        // 空闲链表中恰好有大小为n的空闲空间，直接分配
        if (!list_empty(&(free_array[order])))
        {
            page = le2page(list_next(&(free_array[order])), page_link);
            list_del(list_next(&(free_array[order])));
            SetPageProperty(page);
            nr_free -= n;
            break;
        }

        for (int i = order; i < 16; i++)
        {
            if (!list_empty(&(free_array[i])))
            {
                struct Page *page1 = le2page(list_next(&(free_array[i])), page_link);
                struct Page *page2 = page1 + (POW2(i - 1));

                page1->property = i - 1;
                page2->property = i - 1;

                list_del(list_next(&(free_array[i])));
                list_add(&(free_array[i - 1]), &(page2->page_link));
                list_add(&(free_array[i - 1]), &(page1->page_link));

                break;
            }
        }
    }
    return page;
}
// 释放内存页块
static void buddy_free_pages(struct Page *base, size_t n)
{
    assert(n > 0);
    // 更新空闲空间大小
    nr_free += POW2(base->property);
    // cprintf("base property is %d",base->property);
    struct Page *free_page = base;
    struct Page *free_page_buddy = GET_BUDDY(free_page);
    // 将释放的空间接入到对应的项的空闲链表上
    list_add(&(free_array[free_page->property]), &(free_page->page_link));
    // 当其伙伴空间没有被使用且不大于设定的最大块时
    while (!PageProperty(free_page_buddy) && free_page->property < 14)
    {

        if (free_page_buddy < free_page)
        {
            struct Page *temp;
            free_page->property = 0;
            ClearPageProperty(free_page);
            temp = free_page;
            free_page = free_page_buddy;
            free_page_buddy = temp;
        }

        list_del(&(free_page->page_link));
        list_del(&(free_page_buddy->page_link));

        free_page->property += 1;

        list_add(&(free_array[free_page->property]), &(free_page->page_link));

        free_page_buddy = GET_BUDDY(free_page);
        // cprintf("buddy's property is %d\n",free_page_buddy->property);
    }
    ClearPageProperty(free_page);
    return;
}
static size_t
buddy_nr_free_pages(void)
{
    return nr_free;
}

static void basic_check(void)
{
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);                            // 确保分配的页面不同
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0); // 确保引用计数都为0。

    // 确保物理地址在合理范围内（小于 npage * PGSIZE）
    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);
    // 释放 p0, p1, 和 p2，然后检查 nr_free 是否为3。
    free_page(p0);
    cprintf("p0 free\n");
    free_page(p1);
    cprintf("p1 free\n");
    free_page(p2);
    cprintf("p2 free\n");
    // cprintf("nr_free is %d",nr_free);
    assert(nr_free == 16384);

    assert((p0 = alloc_pages(4)) != NULL);
    assert((p1 = alloc_pages(2)) != NULL);
    assert((p2 = alloc_pages(1)) != NULL);
    cprintf("%p,%p,%p\n", page2pa(p0), page2pa(p1), page2pa(p2));
    free_pages(p0, 4);
    cprintf("p0 free\n");
    free_pages(p1, 2);
    cprintf("p1 free\n");
    free_pages(p2, 1);
    cprintf("p2 free\n");

    assert((p0 = alloc_pages(3)) != NULL);
    assert((p1 = alloc_pages(3)) != NULL);
    free_pages(p0, 3);
    free_pages(p1, 3);

    assert((p0 = alloc_pages(16385)) == NULL);
    assert((p0 = alloc_pages(16384)) != NULL);
    free_pages(p0, 16384);
}

static void buddy_check(void)
{
    basic_check(); // 调用 basic_check 函数，检查基本功能是否正常
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};