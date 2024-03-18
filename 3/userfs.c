#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    BLOCK_SIZE = 512,
    MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
    /** Block memory. */
    char *memory;
    /** How many bytes are occupied. */
    int occupied;
    /** Next block in the file. */
    struct block *next;
    /** Previous block in the file. */
    struct block *prev;

    /* PUT HERE OTHER MEMBERS */
};

struct file {
    /** Double-linked list of file blocks. */
    struct block *block_list;
    /**
     * Last block in the list above for fast access to the end
     * of file.
     */
    struct block *last_block;
    /** How many file descriptors are opened on the file. */
    int refs;
    /** File name. */
    char *name;
    /** Files are stored in a double-linked list. */
    struct file *next;
    struct file *prev;

    /* PUT HERE OTHER MEMBERS */
};

/** List of all files. */
static struct file *file_list = NULL;

struct filedesc {
    struct file *file;

    /* PUT HERE OTHER MEMBERS */
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

enum ufs_error_code
ufs_errno()
{
    return ufs_error_code;
}



int ufs_open(const char *filename, int flags) {
    if (filename == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }


    struct file *curr_file = file_list;
    while (curr_file != NULL) {
        if (strcmp(curr_file->name, filename) == 0) {



            curr_file->refs++;


            for (int i = 0; i < file_descriptor_count; i++) {
                if (file_descriptors[i] == NULL) {
                    file_descriptors[i] = malloc(sizeof(struct filedesc));
                    if (file_descriptors[i] == NULL) {
                        ufs_error_code = UFS_ERR_NO_MEM;
                        return -1;
                    }
                    file_descriptors[i]->file = curr_file;
                    return i;
                }
            }


            if (file_descriptor_count == file_descriptor_capacity) {
                int new_capacity = file_descriptor_capacity == 0 ? 1 : file_descriptor_capacity * 2;
                struct filedesc **new_file_descriptors = realloc(file_descriptors,
                                                                 new_capacity * sizeof(struct filedesc *));
                if (new_file_descriptors == NULL) {
                    ufs_error_code = UFS_ERR_NO_MEM;
                    return -1;
                }
                file_descriptors = new_file_descriptors;
                file_descriptor_capacity = new_capacity;
            }


            file_descriptors[file_descriptor_count] = malloc(sizeof(struct filedesc));
            if (file_descriptors[file_descriptor_count] == NULL) {
                ufs_error_code = UFS_ERR_NO_MEM;
                return -1;
            }
            file_descriptors[file_descriptor_count]->file = curr_file;
            file_descriptor_count++;

            return file_descriptor_count - 1;
        }
        curr_file = curr_file->next;
    }


    if (flags & UFS_CREATE) {

        struct file *new_file = malloc(sizeof(struct file));
        if (new_file == NULL) {
            ufs_error_code = UFS_ERR_NO_MEM;
            return -1;
        }
        new_file->name = strdup(filename);
        if (new_file->name == NULL) {
            ufs_error_code = UFS_ERR_NO_MEM;
            free(new_file);
            return -1;
        }
        new_file->block_list = NULL;
        new_file->last_block = NULL;
        new_file->refs = 1;
        new_file->next = file_list;
        new_file->prev = NULL;
        if (file_list != NULL) {
            file_list->prev = new_file;
        }
        file_list = new_file;


        for (int i = 0; i < file_descriptor_count; i++) {
            if (file_descriptors[i] == NULL) {
                file_descriptors[i] = malloc(sizeof(struct filedesc));
                if (file_descriptors[i] == NULL) {
                    ufs_error_code = UFS_ERR_NO_MEM;
                    free(new_file->name);
                    free(new_file);
                    return -1;
                }
                file_descriptors[i]->file = new_file;
                return i;
            }
        }


        if (file_descriptor_count == file_descriptor_capacity) {
            int new_capacity = file_descriptor_capacity == 0 ? 1 : file_descriptor_capacity * 2;
            struct filedesc **new_file_descriptors = realloc(file_descriptors,
                                                             new_capacity * sizeof(struct filedesc *));
            if (new_file_descriptors == NULL) {
                ufs_error_code = UFS_ERR_NO_MEM;
                free(new_file->name);
                free(new_file);
                return -1;
            }
            file_descriptors = new_file_descriptors;
            file_descriptor_capacity = new_capacity;
        }


        file_descriptors[file_descriptor_count] = malloc(sizeof(struct filedesc));
        if (file_descriptors[file_descriptor_count] == NULL) {
            ufs_error_code = UFS_ERR_NO_MEM;
            free(new_file->name);
            free(new_file);
            return -1;
        }
        file_descriptors[file_descriptor_count]->file = new_file;
        file_descriptor_count++;

        return file_descriptor_count - 1;
    }


    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
}



ssize_t
ufs_write(int fd, const char *buf, size_t size)
{
    if (fd <= 0 || fd > file_descriptor_count) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct filedesc *file_desc = file_descriptors[fd - 1];
    if (file_desc == NULL || file_desc->file == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }


    struct file *file = file_desc->file;


    struct block *new_block = malloc(sizeof(struct block));
    if (new_block == NULL) {
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }
    new_block->memory = malloc(size);
    if (new_block->memory == NULL) {
        free(new_block);
        ufs_error_code = UFS_ERR_NO_MEM;
        return -1;
    }

    memcpy(new_block->memory, buf, size);
    new_block->occupied = size;
    new_block->next = NULL;


    if (file->block_list == NULL) {

        file->block_list = new_block;
        file->last_block = new_block;
    } else {

        file->last_block->next = new_block;
        new_block->prev = file->last_block;
        file->last_block = new_block;
    }

    return size;
}

ssize_t
ufs_read(int fd, char *buf, size_t size)
{
    if (fd <= 0 || fd > file_descriptor_count) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct filedesc *file_desc = file_descriptors[fd - 1];
    if (file_desc == NULL || file_desc->file == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }


    struct file *file = file_desc->file;


    ssize_t total_bytes_read = 0;
    struct block *curr_block = file->block_list;
    while (curr_block != NULL && (size_t)total_bytes_read < size) {
        size_t bytes_to_read = (size_t) curr_block->occupied < (size - (size_t)total_bytes_read) ? (size_t) curr_block->occupied : (size_t)(size - (size_t)total_bytes_read);
        memcpy(buf + total_bytes_read, curr_block->memory, bytes_to_read);
        total_bytes_read += bytes_to_read;
        curr_block = curr_block->next;
    }

    return total_bytes_read;
}



int ufs_close(int fd) {
    if (fd < 0 || fd >= file_descriptor_count || file_descriptors[fd] == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct file *file = file_descriptors[fd]->file;
    file->refs--;

    free(file_descriptors[fd]);
    file_descriptors[fd] = NULL;

    return 0;
}

int ufs_delete(const char *filename) {
    if (filename == NULL) {
        ufs_error_code = UFS_ERR_NO_FILE;
        return -1;
    }

    struct file *curr_file = file_list;
    while (curr_file != NULL) {
        if (strcmp(curr_file->name, filename) == 0) {
            curr_file->refs--;

            if (curr_file->refs == 0) {
                free(curr_file->name);

                if (curr_file->prev != NULL) {
                    curr_file->prev->next = curr_file->next;
                }
                if (curr_file->next != NULL) {
                    curr_file->next->prev = curr_file->prev;
                }
                if (file_list == curr_file) {
                    file_list = curr_file->next;
                }


                struct block *curr_block = curr_file->block_list;
                while (curr_block != NULL) {
                    struct block *next_block = curr_block->next;
                    free(curr_block->memory);
                    free(curr_block);
                    curr_block = next_block;
                }


                curr_file->block_list = NULL;
                curr_file->last_block = NULL;
                curr_file->refs = 0;

                free(curr_file);
            }

            return 0;
        }
        curr_file = curr_file->next;
    }

    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
}



void ufs_destroy() {

    for (int i = 0; i < file_descriptor_count; i++) {
        free(file_descriptors[i]);
    }
    free(file_descriptors);


    struct file *curr_file = file_list;
    while (curr_file != NULL) {
        struct file *next_file = curr_file->next;
        free(curr_file->name);

        struct block *curr_block = curr_file->block_list;
        while (curr_block != NULL) {
            struct block *next_block = curr_block->next;
            free(curr_block->memory);
            free(curr_block);
            curr_block = next_block;
        }

        free(curr_file);
        curr_file = next_file;
    }
}


