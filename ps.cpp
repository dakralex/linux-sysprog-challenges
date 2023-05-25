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

char get_proc_state(pid_t pid) {
    int fd;
    ssize_t nread;
    char buf[BUF_SIZE];
    char state = '\0';
    char proc_state_path[NAME_LIMIT];
    get_proc_path(pid, "stat", proc_state_path);

    // Open and read the proc status information
    if ((fd = open(proc_state_path, O_RDONLY)) == -1) return state;
    if ((nread = read(fd, buf, BUF_SIZE)) == -1) return state;
    buf[nread] = '\0';

    // Extract the information after pid and comm (which is in parentheses)
    char *buf_state_start = strrchr(buf, ')');

    if (buf_state_start != nullptr) {
        state = *(buf_state_start + 2);
    }

    close(fd);

    return state;
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
            info->state = get_proc_state(info->pid);
            if (info->state == '\0') continue;

            printf("pid: %d, state: %c\n", info->pid, info->state);

            // increment bpos
            bpos += procd->d_reclen;
        }
    } while(nread > 0);

    close(fd);

    return 0;
}
