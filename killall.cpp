extern "C" {
    #include <fcntl.h>      /* open syscall, AT_ constants */
    #include <signal.h>     /* kill syscall */
    #include <dirent.h>     /* getdents64 syscall */
    #include <stdlib.h>     /* malloc, free */
    #include <stdio.h>
}

#define PROCFS_MOUNT        "/proc/"    // pre- and suffix with /
#define PROCFS_NAME_FILE    "/comm"     // prefix with /
#define BUF_SIZE            1024

/**
 * Return the length of a given c-style char string. Assumes that the string is
 * terminated with a null-byte character.
 */
size_t strlen(const char *str) {
    const char *ptr = str;

    // traverse the string until a null-byte occurs
    while (*ptr != '\0') ++ptr;

    // get length from pointer positions
    return (ptr - str);
}

/**
 * Returns a concatenated c-style char string.
 */
char* strcat(char *dest, const char *src) {
    //
    char *ptr = dest + strlen(dest);

    // append src to dest until the end
    while (*src != '\0') {
        *ptr++ = *src++;
    }

    // terminate string with null byte
    *ptr = '\0';

    return dest;
}

/**
 * Returns whether the needle was found in the haystack.
 * Implementation of the Knuth-Morris-Pratt algorithm.
 */
bool strstr(char *haystack, const char *needle) {
    while(*haystack) {

        ++haystack;
    }

    return false;
}

/**
 * Get the name of the processes name file.
 * This is not an efficient solution at all.
 */
char* get_proc_comm_path(char d_name[]) {
    size_t length = strlen(PROCFS_MOUNT) +
                    strlen(d_name) +
                    strlen(PROCFS_NAME_FILE);

    char *proc_name = (char *) malloc(sizeof(char*) * length);

    strcat(proc_name, PROCFS_MOUNT);
    strcat(proc_name, d_name);
    strcat(proc_name, PROCFS_NAME_FILE);

    return proc_name;
}

int main() {
    int                     fd;                 /* file descriptor of procfs */
    char                    buf[BUF_SIZE];      /* buffer for reading procfs */
    size_t                  nread;              /* read bytes from getdents64 */
    size_t                  bpos;               /* current buffer pos */
    struct dirent64         *d = nullptr;       /* current dir entry */
    int                     proc_fd;            /* current process fd */
    char                    proc_name[BUF_SIZE];/* process name buffer */
    int                     proc_name_nread;    /* process name length */

    fd = open(PROCFS_MOUNT, O_RDONLY | O_DIRECTORY);

    do {
        nread = getdents64(fd, buf, BUF_SIZE);

        // stop if there is nothing left to read
        if (nread == 0) break;

        // go through all directory entries in procfs
        for (bpos = 0; bpos < nread;) {
            d = (struct dirent64 *) (buf + bpos);

            // skip any entry that is not a folder
            if (d->d_type != DT_DIR) break;

            // skip any folder which does not start with a number
            if (d->d_name[0] < '0' || d->d_name[0] > '9') break;

            // read
            if ((proc_fd = open(get_proc_comm_path(d->d_name), O_RDONLY)) == -1)
                continue;
            if ((proc_name_nread = read(proc_fd, proc_name, BUF_SIZE)) == -1)
                continue;

            proc_name[proc_name_nread] = '\0';

            printf("%s", proc_name);

            // increment bpos
            bpos += d->d_reclen;
        }
    } while(nread > 0);

    /* int kill(pid_t pid, int sig); */
}
