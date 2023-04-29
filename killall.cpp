extern "C" {
    #include <unistd.h>     /* write syscall */
    #include <fcntl.h>      /* open syscall, AT_ constants */
    #include <signal.h>     /* kill syscall */
    #include <dirent.h>     /* getdents64 syscall */
    #include <stdlib.h>     /* malloc, free, strtol */
}

#define FD_STDOUT           1
#define FD_STDERR           2
#define PROCFS_MOUNT        "/proc/"    // pre- and suffix with /
#define BUF_SIZE            1024

/**
 * Return the length of a given c-style char string. Assumes that the string is
 * terminated with a null-byte character.
 */
size_t strlen(const char *str, char end = '\0') {
    const char *ptr = str;

    // traverse the string until a null-byte occurs
    while (*ptr != end) ++ptr;

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
char* strstr(char *haystack, const char *needle) {
    while(*haystack != '\0') {
        const char *str = haystack;
        const char *sstr = needle;

        while (*str && *sstr && (*str == *sstr)) {
            ++str;
            ++sstr;
        }

        if (*sstr == '\0') {
            return haystack;
        }

        ++haystack;
    }

    return nullptr;
}

/**
 * Prints the given string to console.
 */
void print(const char *str = "", int fd = FD_STDOUT) {
    write(fd, str, strlen(str));
}

/**
 * Print the given string as a line to console.
 */
void println(const char *str = "", int fd = FD_STDOUT) {
    print(str, fd);
    print("\n", fd);
}

/**
 * Get the name of the processes name file.
 * This is not an efficient solution at all.
 */
char* get_proc_path(char d_name[], const char file_name[]) {
    size_t length = strlen(PROCFS_MOUNT) +
                    strlen(d_name) +
                    strlen(file_name);

    char *proc_name = (char *) malloc(sizeof(char*) * length);

    strcat(proc_name, PROCFS_MOUNT);
    strcat(proc_name, d_name);
    strcat(proc_name, file_name);

    return proc_name;
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        println("Usage: ./killall NAME", FD_STDERR);
        return -1;
    }

    const char *proc_kill_name = argv[1];

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

            char* proc_info_path = get_proc_path(d->d_name, "/status");

            // read the process name file into the buffer
            if ((proc_fd = open(proc_info_path, O_RDONLY)) == -1)
                continue;
            if ((proc_name_nread = read(proc_fd, proc_name, BUF_SIZE)) == -1)
                continue;

            free(proc_info_path);

            proc_name[strlen(proc_name, '\n')] = '\0';

            bool to_kill = strstr(proc_name, proc_kill_name) != nullptr;

            if (to_kill) {
                pid_t kill_pid = (pid_t) strtol(d->d_name, (char**) nullptr, 10);

                kill(kill_pid, SIGKILL);
            }

            close(proc_fd);

            // increment bpos
            bpos += d->d_reclen;
        }
    } while(nread > 0);

    close(fd);

    return 0;
}
