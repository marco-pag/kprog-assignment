/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#ifndef TMOD_BUFF_H
#define TMOD_BUFF_H

struct tmod_buff;

int tmod_buff_init(struct tmod_buff **buff, const size_t buffs_count,
					const size_t buff_len);
void tmod_buff_destroy(struct tmod_buff *buff);

/* Push and pop returns the number of bytes or zero in case of error */
size_t tmod_buff_push(struct tmod_buff *buff, const char *data, size_t length);

size_t tmod_buff_pop(struct tmod_buff *buff, char **data);

#endif /* TMOD_BUFF_H */
