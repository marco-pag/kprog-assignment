/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#ifndef TMOD_WORKER_H
#define TMOD_WORKER_H

void tmod_worker_body(const char *blk_in, char *blk_out, size_t len, char key);

#endif /* TMOD_WORKER_H */
