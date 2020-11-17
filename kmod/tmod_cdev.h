/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#ifndef TMOD_CDEV_H
#define TMOD_CDEV_H

#include <linux/types.h>

struct cdev_ctx;

int tmod_cdev_create(struct cdev_ctx **ctx, size_t blk_mnum, size_t blk_mlen, char key);
void tmod_cdev_destroy(struct cdev_ctx *ctx);

#endif /* TMOD_CDEV_H */
