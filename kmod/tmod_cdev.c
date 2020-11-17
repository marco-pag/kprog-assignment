/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#include <linux/fs.h>				/* file_operations */
#include <linux/poll.h>				/* poll support */
#include <linux/miscdevice.h>
#include <linux/cdev.h>				/* cdev utils */
#include <linux/slab.h>				/* kmalloc */
#include <linux/mutex.h>
#include <linux/kthread.h>

#include "tmod_buff.h"
#include "tmod_worker.h"

/* Context */
struct cdev_ctx {
	struct mutex m_lock;
	
	/* Misc device */
	struct miscdevice msc_cdev;
	
	/* Count users */
	atomic_t users_cnt_a;
	
	/* Worker thread */
	struct task_struct *worker_p;
	
	/* Buffer components */
	struct tmod_buff *buff_in;
	struct tmod_buff *buff_out;
	
	/* Parameters */
	size_t blk_mlen;
	size_t blk_mnum;
	char key;
	
	/* Wait queues + associated state variable for input buffer */
	wait_queue_head_t blks_in_not_full;
	wait_queue_head_t blks_in_not_empty;
	unsigned int blks_in;
	
	/* Wait queues + associated state variable for output buffer */
	wait_queue_head_t blks_out_not_full;
	wait_queue_head_t blks_out_not_empty;
	unsigned int blks_out;
};

/*--------------------------- Char Device ----------------------------*/

/* 
 * Optimistic approach: assume that most of the time the buffer
 * will be available (not full).
*/
static ssize_t cdev_read(struct file *file, char __user *ubuf, size_t len, loff_t *off)
{
	size_t retval;
	size_t len_cut;
	char *blk;
	struct cdev_ctx *ctx;
	struct miscdevice *misc_dev;
	
	printk(KERN_DEBUG "tmod: cdev read: len = %zu\n", len);
	
	if (!len) {
		return 0;
	}
	
	misc_dev = file->private_data;
	ctx = container_of(misc_dev, struct cdev_ctx, msc_cdev);
	
	/* Check if messages are available in the output buffer */
	mutex_lock(&ctx->m_lock);
	while (ctx->blks_out == 0) {
		/* If no blocks have been submitted return (avoid cat to wait indefinitely) */
		if (ctx->blks_in == 0) {
			mutex_unlock(&ctx->m_lock);
			return 0;
		}
		
		mutex_unlock(&ctx->m_lock);
		if (wait_event_interruptible(ctx->blks_out_not_empty, (ctx->blks_out > 0))) {
			/* if woken up by a signal return */
			printk(KERN_INFO "tmod: proc %u interrupted up by a signal"
					" while waiting in read()\n", (unsigned)current->pid);
			return -ERESTARTSYS;
		}
		mutex_lock(&ctx->m_lock);
	}
	
	/* Get message from the buffer */
	retval = tmod_buff_pop(ctx->buff_out, &blk);
	BUG_ON(!retval);
	
	ctx->blks_out--;
	wake_up_interruptible(&ctx->blks_out_not_full);
	
	mutex_unlock(&ctx->m_lock);
	
	/* Copy the message back into the userspace buffer */
	len_cut = len > retval ? retval : len;
	retval = copy_to_user(ubuf, blk, len_cut);
	if (retval) {
		printk(KERN_ERR "tmod: copy_to_user failed\n");
		kfree(blk);
		return -EFAULT;
	}
	
	kfree(blk);
	return (ssize_t)len_cut;
}

static ssize_t cdev_write(struct file *file, const char __user *ubuf, size_t len, loff_t *off)
{
	size_t retval;
	size_t len_cut;
	char *blk;
	struct cdev_ctx *ctx;
	struct miscdevice *misc_dev;
	
	printk(KERN_DEBUG "tmod: cdev write: len = %zu\n", len);
	
	if (!len) {
		return 0;
	}
	
	misc_dev = file->private_data;
	ctx = container_of(misc_dev, struct cdev_ctx, msc_cdev);
	
	/* 
	 * Copy data from userspace into a newly allocated buffer.
	 * Allocate first, ouside the critical section since the number
	 * of user process is limited to one by a counter
	*/
	len_cut = len > ctx->blk_mlen ? ctx->blk_mlen : len;
	blk = kzalloc(len_cut, GFP_USER);
	if (!blk) {
		printk(KERN_ERR "tmod: unable to allocate mem in write\n");
		return -ENOMEM;
	}
	
	retval = copy_from_user(blk, ubuf, len_cut);
	if (retval) {
		printk(KERN_ERR "tmod: copy_from_user failed\n");
		kfree(blk);
		return -EFAULT;
	}
	
	/* Try to push the message into the input buffer */
	mutex_lock(&ctx->m_lock);
	while (ctx->blks_in >= ctx->blk_mnum) {
		mutex_unlock(&ctx->m_lock);
		if (wait_event_interruptible(ctx->blks_in_not_full, (ctx->blks_in < ctx->blk_mnum))) {
			/* if woken up by a signal return */
			printk(KERN_INFO "tmod: proc %u interrupted up by a signal"
					" while waiting in write()\n", (unsigned)current->pid);
			return -ERESTARTSYS;
		}
		mutex_lock(&ctx->m_lock);
	}
	
	/* Push message (and check consistency) */
	retval = tmod_buff_push(ctx->buff_in, blk, len_cut);
	BUG_ON(!retval);
	
	/* Increment status variable associated with the wait queue and "signal" */
	ctx->blks_in++;
	wake_up_interruptible(&ctx->blks_in_not_empty);
	
	mutex_unlock(&ctx->m_lock);
	
	return (ssize_t)len_cut;	
}

static int cdev_close(struct inode *inode, struct file *file)
{
	struct cdev_ctx *ctx;
	struct miscdevice *misc_dev;	
	
	misc_dev = file->private_data;
	ctx = container_of(misc_dev, struct cdev_ctx, msc_cdev);
	
	printk(KERN_DEBUG "tmod: cdev close\n");
	
	atomic_inc(&ctx->users_cnt_a);
	
	return 0;
}

static int cdev_open(struct inode *inode, struct file *file)
{
	struct cdev_ctx *ctx;
	struct miscdevice *misc_dev;

	printk(KERN_DEBUG "tmod: cdev open\n");	
	
	/* file->private_data is already set to struct miscdevice by the miscdevice framework  */
	misc_dev = file->private_data;
	
	/* Get a pointer to the cdev_ctx container stucture */
	ctx = container_of(misc_dev, struct cdev_ctx, msc_cdev);
	
	/* One user at time */
	/* atomic_dec_and_test() return true if the result is 0, otherwise false */
	if (atomic_dec_and_test(&ctx->users_cnt_a)) {
		atomic_inc(&ctx->users_cnt_a);
		printk(KERN_DEBUG "tmod: too many users\n");	
		return -EBUSY;
	}
	
	return 0;
}

static const char *dev_name_str = "enc_dev";

static const struct file_operations msc_cdev_fops = {
	.owner		= THIS_MODULE,
	.read		= cdev_read,
	.open 		= cdev_open,
	.release 	= cdev_close,
	.write		= cdev_write
};

/*--------------------------------------------------------------------*/

int tmod_worker(void *data)
{
	size_t retval;
	size_t blk_len;
	struct cdev_ctx *ctx;
	char *blk_in;
	char *blk_out;
	
	ctx = (struct cdev_ctx *)data;
	
	while (!kthread_should_stop()) {
		
		/* Wait for input data */
		mutex_lock(&ctx->m_lock);
		while (ctx->blks_in == 0) {
			mutex_unlock(&ctx->m_lock);
			wait_event_interruptible(ctx->blks_in_not_empty, 
									((ctx->blks_in > 0) || kthread_should_stop()));
			if (kthread_should_stop()) {break;}
			mutex_lock(&ctx->m_lock);
		}
		
		/* Get data from the input buffer */
		blk_len = tmod_buff_pop(ctx->buff_in, &blk_in);
		BUG_ON(!blk_len);
		
		ctx->blks_in--;
		wake_up_interruptible(&ctx->blks_in_not_full);
		
		mutex_unlock(&ctx->m_lock);
		
		/* Process data */
		blk_out = kzalloc(blk_len, GFP_USER);
		if (!blk_out) {
			printk(KERN_ERR "tmod: unable to allocate mem in worker thread\n");
			return -ENOMEM;
		}
		
		tmod_worker_body(blk_in, blk_out, blk_len, ctx->key);
		printk(KERN_DEBUG "tmod: worker: block encoded\n");
		
		kfree(blk_in);
		
		/* Put processed data into queue */
		mutex_lock(&ctx->m_lock);
		while (ctx->blks_out >= ctx->blk_mnum) {
			mutex_unlock(&ctx->m_lock);
			wait_event_interruptible(ctx->blks_out_not_full,
									(ctx->blks_out < ctx->blk_mnum) || kthread_should_stop());
			mutex_lock(&ctx->m_lock);
		}
		
		retval = tmod_buff_push(ctx->buff_out, blk_out, blk_len);
		BUG_ON(!retval);
		
		ctx->blks_out++;
		wake_up_interruptible(&ctx->blks_out_not_empty);
		
		mutex_unlock(&ctx->m_lock);
	}
	
	return 0;
}

/*--------------------------------------------------------------------*/

/* 
 * note: called by init in tmod.c
 * 
 * note: no need to use dev_set_drvdata() to store the context in the
 * device handler since it is passed back by the user
 */
int tmod_cdev_create(struct cdev_ctx **ctx, size_t blk_mnum, size_t blk_mlen, char key)
{
	int retval;
	
	*ctx = kzalloc(sizeof(**ctx), GFP_USER);
	if (!ctx) {
		printk(KERN_ERR "tmod: unable to allocate mem in cdev create\n");
		return -ENOMEM;
	}
	
	mutex_init(&(*ctx)->m_lock);
	
	atomic_set(&(*ctx)->users_cnt_a, 2 + 1);
	(*ctx)->blk_mnum = blk_mnum;
	(*ctx)->blk_mlen = blk_mlen;
	(*ctx)->key = key;
	
	(*ctx)->blks_in = 0;
	(*ctx)->blks_out = 0;
	
	init_waitqueue_head(&(*ctx)->blks_in_not_full);
	init_waitqueue_head(&(*ctx)->blks_in_not_empty);
	init_waitqueue_head(&(*ctx)->blks_out_not_full);
	init_waitqueue_head(&(*ctx)->blks_out_not_empty);
	
	/* Init input buffer */
	retval = tmod_buff_init(&(*ctx)->buff_in, blk_mnum, blk_mlen);
	if (retval < 0) {
		printk(KERN_ERR "tmod: unable to initialize the buffer dev\n");
		kfree(*ctx);
		return -ENOMEM;
	}
	
	/* Init output buffer */
	retval = tmod_buff_init(&(*ctx)->buff_out, blk_mnum, blk_mlen);
	if (retval < 0) {
		printk(KERN_ERR "tmod: unable to initialize the buffer dev\n");
		tmod_buff_destroy((*ctx)->buff_in);
		kfree(*ctx);
		return -ENOMEM;
	}
	
	/* Start worker thread */
	(*ctx)->worker_p = kthread_run(tmod_worker, *ctx, "tmod_worker_thread");
	/* test error from pointer */
	if (IS_ERR((*ctx)->worker_p)) {
		printk(KERN_INFO "tmod: proc %i interrupted up by a signal\n", (unsigned)current->pid);
		/* decode error number from the pointer */
		return PTR_ERR(((*ctx)->worker_p));
	}
	
	/* Misc char device */
	(*ctx)->msc_cdev.minor = MISC_DYNAMIC_MINOR;
	(*ctx)->msc_cdev.name = dev_name_str;
	(*ctx)->msc_cdev.fops = &msc_cdev_fops;
	
	retval = misc_register(&(*ctx)->msc_cdev);
	if (retval < 0) {
		printk(KERN_ERR "tmod: failed to register misc dev\n");
		tmod_buff_destroy((*ctx)->buff_in);
		tmod_buff_destroy((*ctx)->buff_out);
		kfree(*ctx);
		return retval;
	}
	
	printk(KERN_INFO "tmod: %s successfully created with %lu buffers of size %lu bytes\n",
			dev_name_str, (*ctx)->blk_mnum, (*ctx)->blk_mlen);
	
	return 0;
}

/* note: called by exit in tmod.c */
void tmod_cdev_destroy(struct cdev_ctx *ctx)
{
	/* Blocks until the thread has stopped */
	kthread_stop(ctx->worker_p);
	
	mutex_destroy(&ctx->m_lock);
	
	tmod_buff_destroy(ctx->buff_in);
	tmod_buff_destroy(ctx->buff_out);
	
	misc_deregister(&ctx->msc_cdev);
	
	kfree(ctx);
}
