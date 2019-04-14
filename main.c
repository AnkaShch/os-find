#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>


/* Exit with error message */
#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Linux dirent structure */
struct linux_dirent {
    long           d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};


/* Command line parameters  */
size_t         PARAM_MASK = 0;

#define __INUM      1 << 0
#define __NAME      1 << 1
#define __SIZE      1 << 2
#define __SIZE_LOW  1 << 3
#define __SIZE_HIGH 1 << 4
#define __NLINKS    1 << 5
#define __EXEC      1 << 6

unsigned long  ino;
char           *name;
unsigned long  size;
unsigned long  size_low;
unsigned long  size_high;
unsigned long  nlinks;
char           *exec;


#define BUF_SIZE 1024

/* Recursive search */
void rsearch(char *str);

/* Handle search result */
void handle(struct linux_dirent *d, char *path_to_file);


int main(int argc, char *argv[]) {
    int i = 1;
    if (argc > 1) {
        int p = strlen(argv[1]);
        while (argv[1][--p] == '/') {
            argv[1][p] = '\0';
        }

        while (++i < argc) {
            if (!strcmp(argv[i], "-inum")) {
                if (++i < argc) {
                    PARAM_MASK |= __INUM;
                    ino = atoi(argv[i]);
                }
            } else if (!strcmp(argv[i], "-name")) {
                if (++i < argc) {
                    PARAM_MASK |= __NAME;
                    name = argv[i];
                }
            } else if (!strcmp(argv[i], "-size")) {
                if (++i < argc) {
                    switch (argv[i][0]) {
                        case '-':
                            PARAM_MASK |= __SIZE_LOW;
                            size_low = atoi(argv[i] + 1);
                            break;
                        case '=':
                            PARAM_MASK |= __SIZE;
                            size = atoi(argv[i] + 1);
                            break;
                        case '+':
                            PARAM_MASK |= __SIZE_HIGH;
                            size_high = atoi(argv[i] + 1);
                            break;
                        default:
                            PARAM_MASK |= __SIZE;
                            size = atoi(argv[i]);
                            break;
                    }
                }
            } else if (!strcmp(argv[i], "-nlinks")) {
                if (++i < argc) {
                    PARAM_MASK |= __NLINKS;
                    nlinks = atoi(argv[i]);
                }
            } else if (!strcmp(argv[i], "-exec")) {
                if (++i < argc) {
                    PARAM_MASK |= __EXEC;
                    exec = argv[i];
                }
            }        
        }
    }

    rsearch(argc > 1 ? argv[1] : ".");
    exit(EXIT_SUCCESS);
}


void rsearch(char *str) {
    int fd, nread;
    char buf[BUF_SIZE];
    char path_to_file[BUF_SIZE];
    struct linux_dirent *d;
    int bpos;
    char d_type;

    fd = open(str, O_RDONLY | O_DIRECTORY);
    if (fd == -1)
        handle_error("open");

    while (true) {
        nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
        if (nread == -1)
            handle_error("getdents");

        if (nread == 0)
            break;

        for (bpos = 0; bpos < nread;) {
            d = (struct linux_dirent *) (buf + bpos);
            d_type = *(buf + bpos + d->d_reclen - 1);

            if (strcmp(d->d_name, ".") != 0 &&
                strcmp(d->d_name, "..") != 0) {

                strcpy(path_to_file, str);
                strcat(path_to_file, "/");
                strcat(path_to_file, d->d_name);

                handle(d, path_to_file);

                if (d_type == DT_DIR) {
                    rsearch(path_to_file);
                }
            }
            bpos += d->d_reclen;
        }
    }
}

void handle(struct linux_dirent *d, char *path_to_file) {
    struct stat sb;
    pid_t child_pid;

    if (lstat(path_to_file, &sb) == -1) {
        handle_error("lstat");
    }

    if ((PARAM_MASK & __INUM) && !(d->d_ino == ino))
        return;
    if ((PARAM_MASK & __NAME) && (strcmp(d->d_name, name)))
        return;
    if ((PARAM_MASK & __SIZE) && !(sb.st_size == size))
        return;
    if ((PARAM_MASK & __SIZE_LOW) && !(sb.st_size < size_low))
        return;
    if ((PARAM_MASK & __SIZE_HIGH) && !(sb.st_size > size_high))
        return;
    if ((PARAM_MASK & __NLINKS) && !(sb.st_nlink == nlinks))
        return;

    if (PARAM_MASK & __EXEC) {
        child_pid = fork();
        switch (child_pid) {
        case -1:
            handle_error("fork");
            break;

        case 0:
            (void)execl(exec, exec, path_to_file, NULL);
            handle_error("exec");
            break;

        default:
            signal(SIGCHLD, SIG_IGN);
            break;
        }
    } else {
        puts(path_to_file);
    }
}
