extern "C" {
    #include <unistd.h>     /* write syscall */
    #include <fcntl.h>      /* open syscall, AT_ constants */
    #include <dirent.h>     /* getdents64 syscall */
    #include <stdlib.h>     /* malloc, free, strtol */
    #include <stdio.h>      /* snprintf */
    #include <string.h>
}

#define PROCFS_MOUNT        "/proc/"    // pre- and suffix with /
#define NAME_LIMIT          256
#define BUF_SIZE            4096
#define PATH_LIMIT          4096

/**
 * Get the name of the processes name file.
 * This is not an efficient solution at all.
 */
void get_proc_path(pid_t pid, const char name[], char *proc_path) {
    snprintf(proc_path, NAME_LIMIT, "/proc/%d/%s", pid, name);
}

struct procInfo {
    pid_t pid;
    char state;
    unsigned long long base_address;
    char exe[PATH_LIMIT];
    char cwd[PATH_LIMIT];
    char **cmdline;
};

/**
 * Returns the state indicated by a char defined in `man 5 proc`.
 */
void get_proc_state(pid_t pid, char &state) {
    int fd;
    ssize_t nread;
    char buf[BUF_SIZE];
    char proc_state_path[NAME_LIMIT];
    get_proc_path(pid, "stat", proc_state_path);

    state = '\0';

    // Open and read the proc status information
    if ((fd = open(proc_state_path, O_RDONLY)) == -1) return;
    if ((nread = read(fd, buf, BUF_SIZE)) == -1) return;

    buf[nread] = '\0';

    // Extract the state from the buf string, which is in the 3rd place
    // The string format is: %d (%s) %c [...]
    // So we need to get the %c which is after a parenthesis
    char *buf_state_start = strrchr(buf, ')');

    if (buf_state_start != nullptr) {
        state = *(buf_state_start + 2);
    }

    close(fd);
}

/**
 * Returns the base address which is the first address mapped to the process.
 */
unsigned long long get_proc_base_address(pid_t pid) {
    int fd;
    ssize_t nread;
    char buf[BUF_SIZE];
    char proc_state_path[NAME_LIMIT];
    get_proc_path(pid, "maps", proc_state_path);

    // Open and read the proc maps information
    if ((fd = open(proc_state_path, O_RDONLY)) == -1) return 0;
    if ((nread = read(fd, buf, BUF_SIZE)) == -1) return 0;
    buf[nread] = '\0';

    // Extract the information after pid and comm (which is in parentheses)
    char *buf_base_start = buf;
    char *buf_base_end = strrchr(buf, '-') - 1;

    if (buf_base_end != nullptr) {
        return strtoull(buf_base_start, &buf_base_end, 10);
    }

    return 0;
}

int main() {
    int                     fd;                 /* file descriptor of procfs */
    char                    buf[BUF_SIZE];      /* buffer for reading procfs */
    ssize_t                 nread = 0u;         /* read bytes from getdents64 */
    ssize_t                 bpos;               /* current buffer pos */
    dirent64                *procd = nullptr;   /* current dir entry */
    procInfo                *info = (procInfo *) malloc(sizeof(procInfo));

    fd = open(PROCFS_MOUNT, O_RDONLY | O_DIRECTORY);

    do {
        nread = getdents64(fd, buf, BUF_SIZE);

        // stop if there is nothing left to read
        if (nread == 0) break;

        // go through all directory entries in procfs
        for (bpos = 0u; bpos < nread;) {
            procd = (struct dirent64 *) (buf + bpos);

            // skip any entry that is not a folder
            if (procd->d_type != DT_DIR) break;

            // skip any folder which does not start with a number
            if (procd->d_name[0] < '0' || procd->d_name[0] > '9') break;

            info->pid = strtoul(procd->d_name, nullptr, 10);
            get_proc_state(info->pid, info->state);
            info->base_address = get_proc_base_address(info->pid);
            if (info->state == '\0') continue;

            printf("pid: %d, state: %c, base address: %llu\n", info->pid, info->state, info->base_address);

            // increment bpos
            bpos += procd->d_reclen;
        }
    } while(nread > 0);

    close(fd);

    return 0;
}
