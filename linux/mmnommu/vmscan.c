/*
 *  linux/mm/vmscan.c
 *
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Swap reorganised 29.12.95, Stephen Tweedie.
 *  kswapd added: 7.1.96  sct
 *  Removed kswapd_ctl limits, and swap out as many pages as needed
 *  to bring the system back to freepages.high: 2.4.97, Rik van Riel.
 *  Version: $Id: vmscan.c,v 1.4 2001/01/05 18:32:16 davidm Exp $
 *  Zone aware kswapd started 02/00, Kanoj Sarcar (kanoj@sgi.com).
 *  Multiqueue VM started 5.8.00, Rik van Riel.
 */

#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/smp_lock.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/file.h>

#include <asm/pgalloc.h>


/*
 * Select the task with maximal swap_cnt and try to swap out a page.
 * N.B. This function returns only 0 or 1.  Return values != 1 from
 * the lower level routines result in continued processing.
 */

static int swap_out(unsigned int priority, int gfp_mask)
{
	return(0);
}


/**
 * reclaim_page -	reclaims one page from the inactive_clean list
 * @zone: reclaim a page from this zone
 *
 * The pages on the inactive_clean can be instantly reclaimed.
 * The tests look impressive, but most of the time we'll grab
 * the first page of the list and exit successfully.
 */
struct page * reclaim_page(zone_t * zone)
{
	struct page * page = NULL;
	struct list_head * page_lru;
	int maxscan;

	/*
	 * We only need the pagemap_lru_lock if we don't reclaim the page,
	 * but we have to grab the pagecache_lock before the pagemap_lru_lock
	 * to avoid deadlocks and most of the time we'll succeed anyway.
	 */
	spin_lock(&pagecache_lock);
	spin_lock(&pagemap_lru_lock);
	maxscan = zone->inactive_clean_pages;
	while ((page_lru = zone->inactive_clean_list.prev) !=
			&zone->inactive_clean_list && maxscan--) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageInactiveClean(page)) {
			printk("VM: reclaim_page, wrong page on list.\n");
			list_del(page_lru);
			page->zone->inactive_clean_pages--;
			continue;
		}

		/* Page is or was in use?  Move it to the active list. */
		if (PageTestandClearReferenced(page) || page->age > 0 ||
				(!page->buffers && page_count(page) > 1)) {
			del_page_from_inactive_clean_list(page);
			add_page_to_active_list(page);
			continue;
		}

		/* The page is dirty, or locked, move to inactive_dirty list. */
		if (page->buffers || PageDirty(page) || TryLockPage(page)) {
			del_page_from_inactive_clean_list(page);
			add_page_to_inactive_dirty_list(page);
			continue;
		}

		/* OK, remove the page from the caches. */
                if (PageSwapCache(page)) {
			__delete_from_swap_cache(page);
			goto found_page;
		}

		if (page->mapping) {
			__remove_inode_page(page);
			goto found_page;
		}

		/* We should never ever get here. */
		printk(KERN_ERR "VM: reclaim_page, found unknown page\n");
		list_del(page_lru);
		zone->inactive_clean_pages--;
		UnlockPage(page);
	}
	/* Reset page pointer, maybe we encountered an unfreeable page. */
	page = NULL;
	goto out;

found_page:
	del_page_from_inactive_clean_list(page);
	UnlockPage(page);
	page->age = PAGE_AGE_START;
	if (page_count(page) != 1)
		printk("VM: reclaim_page, found page with count %d!\n",
				page_count(page));
out:
	spin_unlock(&pagemap_lru_lock);
	spin_unlock(&pagecache_lock);
	memory_pressure++;
	return page;
}

/**
 * page_launder - clean dirty inactive pages, move to inactive_clean list
 * @gfp_mask: what operations we are allowed to do
 * @sync: should we wait synchronously for the cleaning of pages
 *
 * When this function is called, we are most likely low on free +
 * inactive_clean pages. Since we want to refill those pages as
 * soon as possible, we'll make two loops over the inactive list,
 * one to move the already cleaned pages to the inactive_clean lists
 * and one to (often asynchronously) clean the dirty inactive pages.
 *
 * In situations where kswapd cannot keep up, user processes will
 * end up calling this function. Since the user process needs to
 * have a page before it can continue with its allocation, we'll
 * do synchronous page flushing in that case.
 *
 * This code is heavily inspired by the FreeBSD source code. Thanks
 * go out to Matthew Dillon.
 */
#define MAX_LAUNDER 		(4 * (1 << page_cluster))
int page_launder(int gfp_mask, int sync)
{
	int launder_loop, maxscan, cleaned_pages, maxlaunder;
	int can_get_io_locks;
	struct list_head * page_lru;
	struct page * page;

	/*
	 * We can only grab the IO locks (eg. for flushing dirty
	 * buffers to disk) if __GFP_IO is set.
	 */
	can_get_io_locks = gfp_mask & __GFP_IO;

	launder_loop = 0;
	maxlaunder = 0;
	cleaned_pages = 0;

dirty_page_rescan:
	spin_lock(&pagemap_lru_lock);
	maxscan = nr_inactive_dirty_pages;
	while ((page_lru = inactive_dirty_list.prev) != &inactive_dirty_list &&
				maxscan-- > 0) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageInactiveDirty(page)) {
			printk("VM: page_launder, wrong page on list.\n");
			list_del(page_lru);
			nr_inactive_dirty_pages--;
			page->zone->inactive_dirty_pages--;
			continue;
		}

		/* Page is or was in use?  Move it to the active list. */
		if (PageTestandClearReferenced(page) || page->age > 0 ||
				(!page->buffers && page_count(page) > 1) ||
				page_ramdisk(page)) {
			del_page_from_inactive_dirty_list(page);
			add_page_to_active_list(page);
			continue;
		}

		/*
		 * The page is locked. IO in progress?
		 * Move it to the back of the list.
		 */
		if (TryLockPage(page)) {
			list_del(page_lru);
			list_add(page_lru, &inactive_dirty_list);
			continue;
		}

		/*
		 * Dirty swap-cache page? Write it out if
		 * last copy..
		 */
		if (PageDirty(page)) {
			int (*writepage)(struct page *) = page->mapping->a_ops->writepage;
			int result;

			if (!writepage)
				goto page_active;

			/* First time through? Move it to the back of the list */
			if (!launder_loop) {
				list_del(page_lru);
				list_add(page_lru, &inactive_dirty_list);
				UnlockPage(page);
				continue;
			}

			/* OK, do a physical asynchronous write to swap.  */
			ClearPageDirty(page);
			page_cache_get(page);
			spin_unlock(&pagemap_lru_lock);

			result = writepage(page);
			page_cache_release(page);

			/* And re-start the thing.. */
			spin_lock(&pagemap_lru_lock);
			if (result != 1)
				continue;
			/* writepage refused to do anything */
			set_page_dirty(page);
			goto page_active;
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we either free
		 * the page (in case it was a buffercache only page) or we
		 * move the page to the inactive_clean list.
		 *
		 * On the first round, we should free all previously cleaned
		 * buffer pages
		 */
		if (page->buffers) {
			int wait, clearedbuf;
			int freed_page = 0;
			/*
			 * Since we might be doing disk IO, we have to
			 * drop the spinlock and take an extra reference
			 * on the page so it doesn't go away from under us.
			 */
			del_page_from_inactive_dirty_list(page);
			page_cache_get(page);
			spin_unlock(&pagemap_lru_lock);

			/* Will we do (asynchronous) IO? */
			if (launder_loop && maxlaunder == 0 && sync)
				wait = 2;	/* Synchrounous IO */
			else if (launder_loop && maxlaunder-- > 0)
				wait = 1;	/* Async IO */
			else
				wait = 0;	/* No IO */

			/* Try to free the page buffers. */
			clearedbuf = try_to_free_buffers(page, wait);

			/*
			 * Re-take the spinlock. Note that we cannot
			 * unlock the page yet since we're still
			 * accessing the page_struct here...
			 */
			spin_lock(&pagemap_lru_lock);

			/* The buffers were not freed. */
			if (!clearedbuf) {
				add_page_to_inactive_dirty_list(page);

			/* The page was only in the buffer cache. */
			} else if (!page->mapping) {
				atomic_dec(&buffermem_pages);
				freed_page = 1;
				cleaned_pages++;

			/* The page has more users besides the cache and us. */
			} else if (page_count(page) > 2) {
				add_page_to_active_list(page);

			/* OK, we "created" a freeable page. */
			} else /* page->mapping && page_count(page) == 2 */ {
				add_page_to_inactive_clean_list(page);
				cleaned_pages++;
			}

			/*
			 * Unlock the page and drop the extra reference.
			 * We can only do it here because we ar accessing
			 * the page struct above.
			 */
			UnlockPage(page);
			page_cache_release(page);

			/* 
			 * If we're freeing buffer cache pages, stop when
			 * we've got enough free memory.
			 */
			if (freed_page && !free_shortage())
				break;
			continue;
		} else if (page->mapping && !PageDirty(page)) {
			/*
			 * If a page had an extra reference in
			 * deactivate_page(), we will find it here.
			 * Now the page is really freeable, so we
			 * move it to the inactive_clean list.
			 */
			del_page_from_inactive_dirty_list(page);
			add_page_to_inactive_clean_list(page);
			UnlockPage(page);
			cleaned_pages++;
		} else {
page_active:
			/*
			 * OK, we don't know what to do with the page.
			 * It's no use keeping it here, so we move it to
			 * the active list.
			 */
			del_page_from_inactive_dirty_list(page);
			add_page_to_active_list(page);
			UnlockPage(page);
		}
	}
	spin_unlock(&pagemap_lru_lock);

	/*
	 * If we don't have enough free pages, we loop back once
	 * to queue the dirty pages for writeout. When we were called
	 * by a user process (that /needs/ a free page) and we didn't
	 * free anything yet, we wait synchronously on the writeout of
	 * MAX_SYNC_LAUNDER pages.
	 *
	 * We also wake up bdflush, since bdflush should, under most
	 * loads, flush out the dirty pages before we have to wait on
	 * IO.
	 */
	if (can_get_io_locks && !launder_loop && free_shortage()) {
		launder_loop = 1;
		/* If we cleaned pages, never do synchronous IO. */
		if (cleaned_pages)
			sync = 0;
		/* We only do a few "out of order" flushes. */
		maxlaunder = MAX_LAUNDER;
		/* Kflushd takes care of the rest. */
		wakeup_bdflush(0);
		goto dirty_page_rescan;
	}

	/* Return the number of pages moved to the inactive_clean list. */
	return cleaned_pages;
}

/**
 * refill_inactive_scan - scan the active list and find pages to deactivate
 * @priority: the priority at which to scan
 * @oneshot: exit after deactivating one page
 *
 * This function will scan a portion of the active list to find
 * unused pages, those pages will then be moved to the inactive list.
 */
int refill_inactive_scan(unsigned int priority, int oneshot)
{
	struct list_head * page_lru;
	struct page * page;
	int maxscan, page_active = 0;
	int ret = 0;

	/* Take the lock while messing with the list... */
	spin_lock(&pagemap_lru_lock);
	maxscan = nr_active_pages >> priority;
	while (maxscan-- > 0 && (page_lru = active_list.prev) != &active_list) {
		page = list_entry(page_lru, struct page, lru);

		/* Wrong page on list?! (list corruption, should not happen) */
		if (!PageActive(page)) {
			printk("VM: refill_inactive, wrong page on list.\n");
			list_del(page_lru);
			nr_active_pages--;
			continue;
		}

		/* Do aging on the pages. */
		if (PageTestandClearReferenced(page)) {
			age_page_up_nolock(page);
			page_active = 1;
		} else {
			age_page_down_ageonly(page);
			/*
			 * Since we don't hold a reference on the page
			 * ourselves, we have to do our test a bit more
			 * strict then deactivate_page(). This is needed
			 * since otherwise the system could hang shuffling
			 * unfreeable pages from the active list to the
			 * inactive_dirty list and back again...
			 *
			 * SUBTLE: we can have buffer pages with count 1.
			 */
			if (page->age == 0 && page_count(page) <=
						(page->buffers ? 2 : 1)) {
				deactivate_page_nolock(page);
				page_active = 0;
			} else {
				page_active = 1;
			}
		}
		/*
		 * If the page is still on the active list, move it
		 * to the other end of the list. Otherwise it was
		 * deactivated by age_page_down and we exit successfully.
		 */
		if (page_active || PageActive(page)) {
			list_del(page_lru);
			list_add(page_lru, &active_list);
		} else {
			ret = 1;
			if (oneshot)
				break;
		}
	}
	spin_unlock(&pagemap_lru_lock);

	return ret;
}

/*
 * Check if there are zones with a severe shortage of free pages,
 * or if all zones have a minor shortage.
 */
int free_shortage(void)
{
	pg_data_t *pgdat = pgdat_list;
	int sum = 0;
	int freeable = nr_free_pages() + nr_inactive_clean_pages();
	int freetarget = freepages.high + inactive_target / 3;

	/* Are we low on free pages globally? */
	if (freeable < freetarget)
		return freetarget - freeable;

	/* If not, are we very low on any particular zone? */
	do {
		int i;
		for(i = 0; i < MAX_NR_ZONES; i++) {
			zone_t *zone = pgdat->node_zones+ i;
			if (zone->size && (zone->inactive_clean_pages +
					zone->free_pages < zone->pages_min+1)) {
				/* + 1 to have overlap with alloc_pages() !! */
				sum += zone->pages_min + 1;
				sum -= zone->free_pages;
				sum -= zone->inactive_clean_pages;
			}
		}
		pgdat = pgdat->node_next;
	} while (pgdat);

	return sum;
}

/*
 * How many inactive pages are we short?
 */
int inactive_shortage(void)
{
	int shortage = 0;

	shortage += freepages.high;
	shortage += inactive_target;
	shortage -= nr_free_pages();
	shortage -= nr_inactive_clean_pages();
	shortage -= nr_inactive_dirty_pages;

	if (shortage > 0)
		return shortage;

	return 0;
}

/*
 * We need to make the locks finer granularity, but right
 * now we need this so that we can do page allocations
 * without holding the kernel lock etc.
 *
 * We want to try to free "count" pages, and we want to 
 * cluster them so that we get good swap-out behaviour.
 *
 * OTOH, if we're a user process (and not kswapd), we
 * really care about latency. In that case we don't try
 * to free too many pages.
 */
static int refill_inactive(unsigned int gfp_mask, int user)
{
	int priority, count, start_count, made_progress;

	count = inactive_shortage() + free_shortage();
	if (user)
		count = (1 << page_cluster);
	start_count = count;

	/* Always trim SLAB caches when memory gets low. */
	kmem_cache_reap(gfp_mask);

	priority = 6;
	do {
		made_progress = 0;

		if (current->need_resched) {
			__set_current_state(TASK_RUNNING);
			schedule();
		}

		while (refill_inactive_scan(priority, 1)) {
			made_progress = 1;
			if (--count <= 0)
				goto done;
		}

		/*
		 * don't be too light against the d/i cache since
	   	 * refill_inactive() almost never fail when there's
	   	 * really plenty of memory free. 
		 */
		shrink_dcache_memory(priority, gfp_mask);
		shrink_icache_memory(priority, gfp_mask);

		/*
		 * Then, try to page stuff out..
		 */
		while (swap_out(priority, gfp_mask)) {
			made_progress = 1;
			if (--count <= 0)
				goto done;
		}

		/*
		 * If we either have enough free memory, or if
		 * page_launder() will be able to make enough
		 * free memory, then stop.
		 */
		if (!inactive_shortage() || !free_shortage())
			goto done;

		/*
		 * Only switch to a lower "priority" if we
		 * didn't make any useful progress in the
		 * last loop.
		 */
		if (!made_progress)
			priority--;
	} while (priority >= 0);

	/* Always end on a refill_inactive.., may sleep... */
	while (refill_inactive_scan(0, 1)) {
		if (--count <= 0)
			goto done;
	}

done:
	return (count < start_count);
}

static int do_try_to_free_pages(unsigned int gfp_mask, int user)
{
	int ret = 0;

	/*
	 * If we're low on free pages, move pages from the
	 * inactive_dirty list to the inactive_clean list.
	 *
	 * Usually bdflush will have pre-cleaned the pages
	 * before we get around to moving them to the other
	 * list, so this is a relatively cheap operation.
	 */
	if (free_shortage() || nr_inactive_dirty_pages > nr_free_pages() +
			nr_inactive_clean_pages())
		ret += page_launder(gfp_mask, user);

	/*
	 * If needed, we move pages from the active list
	 * to the inactive list. We also "eat" pages from
	 * the inode and dentry cache whenever we do this.
	 */
	if (free_shortage() || inactive_shortage()) {
		shrink_dcache_memory(6, gfp_mask);
		shrink_icache_memory(6, gfp_mask);
		ret += refill_inactive(gfp_mask, user);
	} else {
		/*
		 * Reclaim unused slab cache memory.
		 */
		kmem_cache_reap(gfp_mask);
		ret = 1;
	}

	return ret;
}

DECLARE_WAIT_QUEUE_HEAD(kswapd_wait);
DECLARE_WAIT_QUEUE_HEAD(kswapd_done);
struct task_struct *kswapd_task;

/*
 * The background pageout daemon, started as a kernel thread
 * from the init process. 
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 */
int kswapd(void *unused)
{
	struct task_struct *tsk = current;

	tsk->session = 1;
	tsk->pgrp = 1;
	strcpy(tsk->comm, "kswapd");
	sigfillset(&tsk->blocked);
	kswapd_task = tsk;
	
	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC;

	/*
	 * Kswapd main loop.
	 */
	for (;;) {
		static int recalc = 0;

		/* If needed, try to free some memory. */
		if (inactive_shortage() || free_shortage()) {
			int wait = 0;
			/* Do we need to do some synchronous flushing? */
			if (waitqueue_active(&kswapd_done))
				wait = 1;
			do_try_to_free_pages(GFP_KSWAPD, wait);
		}

		/*
		 * Do some (very minimal) background scanning. This
		 * will scan all pages on the active list once
		 * every minute. This clears old referenced bits
		 * and moves unused pages to the inactive list.
		 */
		refill_inactive_scan(6, 0);

		/* Once a second, recalculate some VM stats. */
		if (time_after(jiffies, recalc + HZ)) {
			recalc = jiffies;
			recalculate_vm_stats();
		}

		/*
		 * Wake up everybody waiting for free memory
		 * and unplug the disk queue.
		 */
		wake_up_all(&kswapd_done);
		run_task_queue(&tq_disk);

		/* 
		 * We go to sleep if either the free page shortage
		 * or the inactive page shortage is gone. We do this
		 * because:
		 * 1) we need no more free pages   or
		 * 2) the inactive pages need to be flushed to disk,
		 *    it wouldn't help to eat CPU time now ...
		 *
		 * We go to sleep for one second, but if it's needed
		 * we'll be woken up earlier...
		 */
		if (!free_shortage() || !inactive_shortage()) {
			interruptible_sleep_on_timeout(&kswapd_wait, HZ);
		/*
		 * If we couldn't free enough memory, we see if it was
		 * due to the system just not having enough memory.
		 * If that is the case, the only solution is to kill
		 * a process (the alternative is enternal deadlock).
		 *
		 * If there still is enough memory around, we just loop
		 * and try free some more memory...
		 */
		} else if (out_of_memory()) {
			oom_kill();
		}
	}
}

void wakeup_kswapd(int block)
{
	DECLARE_WAITQUEUE(wait, current);

	if (current == kswapd_task)
		return;

	if (!block) {
		if (waitqueue_active(&kswapd_wait))
			wake_up(&kswapd_wait);
		return;
	}

	/*
	 * Kswapd could wake us up before we get a chance
	 * to sleep, so we have to be very careful here to
	 * prevent SMP races...
	 */
	__set_current_state(TASK_UNINTERRUPTIBLE);
	add_wait_queue(&kswapd_done, &wait);

	if (waitqueue_active(&kswapd_wait))
		wake_up(&kswapd_wait);
	schedule();

	remove_wait_queue(&kswapd_done, &wait);
	__set_current_state(TASK_RUNNING);
}

/*
 * Called by non-kswapd processes when they want more
 * memory but are unable to sleep on kswapd because
 * they might be holding some IO locks ...
 */
int try_to_free_pages(unsigned int gfp_mask)
{
	int ret = 1;

	if (gfp_mask & __GFP_WAIT) {
		current->flags |= PF_MEMALLOC;
		ret = do_try_to_free_pages(gfp_mask, 1);
		current->flags &= ~PF_MEMALLOC;
	}

	return ret;
}

DECLARE_WAIT_QUEUE_HEAD(kreclaimd_wait);
/*
 * Kreclaimd will move pages from the inactive_clean list to the
 * free list, in order to keep atomic allocations possible under
 * all circumstances. Even when kswapd is blocked on IO.
 */
int kreclaimd(void *unused)
{
	struct task_struct *tsk = current;
	pg_data_t *pgdat;

	tsk->session = 1;
	tsk->pgrp = 1;
	strcpy(tsk->comm, "kreclaimd");
	sigfillset(&tsk->blocked);
	current->flags |= PF_MEMALLOC;

	while (1) {

		/*
		 * We sleep until someone wakes us up from
		 * page_alloc.c::__alloc_pages().
		 */
		interruptible_sleep_on(&kreclaimd_wait);

		/*
		 * Move some pages from the inactive_clean lists to
		 * the free lists, if it is needed.
		 */
		pgdat = pgdat_list;
		do {
			int i;
			for(i = 0; i < MAX_NR_ZONES; i++) {
				zone_t *zone = pgdat->node_zones + i;
				if (!zone->size)
					continue;

				while (zone->free_pages < zone->pages_low) {
					struct page * page;
					page = reclaim_page(zone);
					if (!page)
						break;
					__free_page(page);
				}
			}
			pgdat = pgdat->node_next;
		} while (pgdat);
	}
}


static int __init kswapd_init(void)
{
	printk("Starting kswapd v1.8\n");
	swap_setup();
	kernel_thread(kswapd, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	kernel_thread(kreclaimd, NULL, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
	return 0;
}

module_init(kswapd_init)
