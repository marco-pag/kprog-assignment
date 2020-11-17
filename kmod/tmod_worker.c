/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#include <linux/delay.h>		/* usleep_range() */
#include <linux/types.h>

#include "tmod_worker.h"

void tmod_worker_body(const char *blk_in, char *blk_out, size_t len, char key)
{
	size_t cursor = 0;
	
	if (!blk_in || !blk_out) {
		return;
	}
	
	/* Simplest encoder (XOR) */
	while (cursor < len) {
		/* Simulate hardware processing time */
		usleep_range(100, 101);
		
		blk_out[cursor] = blk_in[cursor] ^ key;
		cursor++;
	}
}
