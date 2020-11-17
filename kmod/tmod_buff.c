/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#include <linux/slab.h>
#include <linux/list.h>

struct tmod_buff {
	size_t buff_mlen;
	size_t buffs_mcount;
	size_t buffs_count;
	struct list_head buffs_head;
};

struct list_node {
	char *data;
	size_t length;
	struct list_head list_nav;
};

int tmod_buff_init(struct tmod_buff **buff, const size_t buffs_mcount,
					const size_t buff_mlen)
{
	*buff = kzalloc(sizeof(**buff), GFP_KERNEL);
	if (!(*buff)) {
		printk(KERN_ALERT "tmod: could not allocate memory for the buffer\n");
		return -ENOMEM;
	}
	
	(*buff)->buff_mlen = buff_mlen;
	(*buff)->buffs_mcount = buffs_mcount;
	(*buff)->buffs_count = 0;
	
	/* Init list */
	INIT_LIST_HEAD(&(*buff)->buffs_head);
	
	return 0;
};

void tmod_buff_destroy(struct tmod_buff *buff)
{
	/* list navs */
	struct list_head *cursor, *tmp;
	/* list node */
	struct list_node *node;

	/* Traverse and delete nodes in the list */
	list_for_each_safe(cursor, tmp, &buff->buffs_head) {
		
		/* Get node */
		node = list_entry(cursor, struct list_node, list_nav); //HERE
		
		/* delete node */
		list_del(cursor);
		
		/* Delete node's internal data */
		kfree(node);
	}
}

size_t tmod_buff_push(struct tmod_buff *buff, const char *data, size_t length)
{
	struct list_node *node;
	
	if (buff->buffs_count == buff->buffs_mcount || length > buff->buff_mlen) {
		return 0;
	}
	
	node = kzalloc(sizeof(*node), GFP_KERNEL);
	
	node->data = (char *)data;
	node->length = length;
	
	list_add_tail(&node->list_nav, &buff->buffs_head);
	buff->buffs_count++;

	return length;
}

size_t tmod_buff_pop(struct tmod_buff *buff, char **data)
{
	struct list_node *node;
	size_t length;
	
	if (!buff->buffs_count) {
		return 0;
	}
	
	BUG_ON(list_empty(&buff->buffs_head));

	/* Get first node (next to the head) */
	node = list_entry(buff->buffs_head.next, struct list_node, list_nav);
	
	*data = node->data;
	length = node->length;
	
	list_del(buff->buffs_head.next);
	buff->buffs_count--;
	kfree(node);
	
	return length;
}
