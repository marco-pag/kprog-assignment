/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/init.h>				/* module init and exit */

#include "tmod_cdev.h"

MODULE_AUTHOR("Marco Pagani");
MODULE_DESCRIPTION("Test module for kprog exam");
MODULE_LICENSE("GPL");

/*------------------------------Module--------------------------------*/

/* Parameter for number of messages */
static unsigned long blk_mnum = 8;
module_param(blk_mnum, ulong, S_IRUGO);

/* Parameter for messages length */
static unsigned long blk_mlen = 64;
module_param(blk_mlen, ulong, S_IRUGO);

/* Parameter for XOR key*/
static char key = 'k';
module_param(key, byte, S_IRUGO);

struct cdev_ctx *ctx;

/*--------------------------------------------------------------------*/

static int __init tmod_init(void)
{
	int retval;
	
	retval = tmod_cdev_create(&ctx, blk_mnum, blk_mlen, key);
	if (retval) {
		printk(KERN_ALERT "tmod: failed to register device\n");
	} else {
		printk(KERN_INFO "tmod: module loaded\n");
	}
	
	return retval;
}

static void __exit tmod_exit(void)
{
	tmod_cdev_destroy(ctx);
}

module_init(tmod_init);
module_exit(tmod_exit);
