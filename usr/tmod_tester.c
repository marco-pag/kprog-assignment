/*
 * Copyright (C) 2018, Marco Pagani.
 * <marco.pag(at)outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define BLK_MLEN 64

static const char key = 'k';
static const char *dev_path = "/dev/enc_dev";

struct file_args {
	int fd;
	int dev_fd;
	off_t size;
};

/* Write data file into device file */
void *writer_body(void *ptr)
{
	off_t len;
	struct file_args *f_args;
	char blk[BLK_MLEN];
	
	f_args = (struct file_args *)ptr;
	
	while (len = read(f_args->fd, &blk, BLK_MLEN), len > 0) {
		len = write(f_args->dev_fd, &blk, len);
		if (len < 0) {
			printf("tmod_tester: writer_body write");
			break;
		}
	}
	
	return NULL;
}

/* Read data from device to data file */
void *reader_body(void *ptr)
{
	off_t len;
	struct file_args *f_args;
	char blk[BLK_MLEN];
	
	f_args = (struct file_args *)ptr;
	
	/* Write before read */
	sleep(1);
	
	while (len = read(f_args->dev_fd, &blk, BLK_MLEN), len > 0) {
		len = write(f_args->fd, &blk, len);
		if (len < 0) {
			printf("tmod_tester: reader_body write\n");
			break;
		}
	}
	
	return NULL;
}

int check_enc(struct file_args *data_in_f, struct file_args *data_out_f)
{
	char src;
	char enc;
	char dec;
	off_t cursor = 0;
	
	while (cursor < data_in_f->size) { 
		if(	read(data_in_f->fd, &src, 1) <= 0 ||
			read(data_out_f->fd, &enc, 1) <= 0 ) {
			printf("tmod_tester: access error on check\n");
			return -1;
		}
		dec = enc ^ key;
		if (dec != src) {
			printf("tmod_tester: char mismatch at byte %li: %c -> %c\n",
					cursor, src, dec);
			return -1;
		}
		cursor++;
	}
	
	return 0;
}

int main(int argc, char **argv)
{
	int retval = 0;
	struct stat sb;
	
	/* Data input file */
	struct file_args *data_in_f = NULL;
	
	/* Data output file */
	struct file_args *data_out_f = NULL;
	
	pthread_t reader_tr;
	pthread_t writer_tr;
	
	if (argc <= 2) {
		perror("tmod_tester: missing data files paths");
		return -1;
	}
	
	data_in_f = calloc(1, sizeof(*data_in_f));
	if (!data_in_f) {
		perror("tmod_tester: on malloc");
		retval = -1;
		goto err_mem;
	}
	
	data_out_f = calloc(1, sizeof(*data_out_f));
	if (!data_out_f) {
		perror("tmod_tester: on malloc");
		retval = -1;
		goto err_mem;
	}
	
	/* Open input data file */
	data_in_f->fd = open(argv[2], O_RDONLY);
	if (data_in_f->fd < 0) {
		perror("tmod_tester: on open data in file");
		retval = -1;
		goto err_mem;
	}
	
	/* Get file size */
	if (fstat(data_in_f->fd, &sb) < 0) {
		perror ("tmod_tester: on fstat");
		return -1;
	}
	data_in_f->size = sb.st_size;
	
	/* Open output data file */
	data_out_f->fd = open(argv[1], O_CREAT | O_RDWR | O_TRUNC);
	if (data_out_f->fd < 0) {
		perror("tmod_tester: on open data out file");
		retval = -1;
		goto err_file;
	}
	
	/* Open device file */
	data_in_f->dev_fd = open(dev_path, O_RDWR);
	if (data_in_f->dev_fd < 0) {
		perror("tmod_tester: on open device file");
		retval = -1;
		goto err_file;
	}
	
	data_out_f->dev_fd = data_in_f->dev_fd;
	
	/* Create workers */
	pthread_create(&writer_tr, NULL, writer_body, (void *)data_in_f);
	pthread_create(&reader_tr, NULL, reader_body, (void *)data_out_f);
	
	/* Wait for workers*/
	pthread_join(writer_tr, NULL);
	pthread_join(reader_tr, NULL);
	
	/* Rewind file position and check */
	lseek(data_in_f->fd, 0, SEEK_SET);
	lseek(data_out_f->fd, 0, SEEK_SET);
	
	if (!check_enc(data_in_f, data_out_f)) {
		printf("tmod_tester: encryption done\n");
	} else {
		printf("tmod_tester: encryption error\n");
		retval = -1;
	}
	
	/* flush buffers */
	sync();
	
err_file:
	if (data_in_f->dev_fd)
		close(data_in_f->dev_fd);

	if (data_in_f->fd)
		close(data_in_f->fd);
	
	if (data_out_f->fd)
		close(data_out_f->fd);

err_mem:
	if (data_in_f)
		free(data_in_f);
	
	if (data_out_f)
		free(data_out_f);
	
	return retval;
}
