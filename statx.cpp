extern "C" {
    #include <unistd.h>     /* write syscall */
    #include <fcntl.h>      /* AT_ constants */
    #include <sys/stat.h>   /* statx syscall, statx struct */
}

#define FD_STDOUT       1
#define FD_STDERR       2
#define STATX_FLAGS     AT_SYMLINK_NOFOLLOW
#define OWNER_MASK      STATX_UID | STATX_GID
#define SIZE_MASK       STATX_SIZE
#define MODE_MASK       STATX_MODE

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
 * Convert an unsigned integer to a c-style string.
 */
void uint_to_str(unsigned int num, char str[]) {
    int i = 0;

    do {
        str[i++] = num % 10 + '0';
        num /= 10;
    } while (num > 0);

    str[i] = '\0';

    int len = i;
    for (i = 0; i < len / 2; ++i) {
        char tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
}

/**
 * Wrapper for statx syscall with error message
 */
int call_statx(const char pathname[], unsigned int mask,
                struct statx *statxbuf) {
    int ret = statx(AT_FDCWD, pathname, STATX_FLAGS, mask, statxbuf);

    if (ret != 0) {
        println("Error: Syscall statx() failed", FD_STDERR);
    }

    return ret;
}

/**
 * Convert the statx_mode to a ls-style permissions string
 */
void mode_to_string(mode_t mode, char perms[]) {
    perms[0] = mode & S_IRUSR ? 'r' : '-';
    perms[1] = mode & S_IWUSR ? 'w' : '-';
    perms[2] = mode & S_IXUSR ? 'x' : '-';
    perms[3] = mode & S_IRGRP ? 'r' : '-';
    perms[4] = mode & S_IWGRP ? 'w' : '-';
    perms[5] = mode & S_IXGRP ? 'x' : '-';
    perms[6] = mode & S_IROTH ? 'r' : '-';
    perms[7] = mode & S_IWOTH ? 'w' : '-';
    perms[8] = mode & S_IXOTH ? 'x' : '-';
    perms[9] = '\0';
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        println("Usage: ./statx FILE", FD_STDERR);
        return -1;
    }

    const char *pathname = argv[1];
    struct statx ownerstat;
    struct statx sizestat;
    struct statx modestat;

    if (call_statx(pathname, OWNER_MASK, &ownerstat) != 0) return -1;
    if (call_statx(pathname, SIZE_MASK, &sizestat) != 0) return -1;
    if (call_statx(pathname, MODE_MASK, &modestat) != 0) return -1;

    // Assuming u32 for uid & gid and u64 for size
    char uid[10];
    char gid[10];
    char size[20];
    char perms[10];

    uint_to_str(ownerstat.stx_uid, uid);
    uint_to_str(ownerstat.stx_gid, gid);
    uint_to_str(sizestat.stx_size, size);
    mode_to_string(modestat.stx_mode, perms);

    print("UID: ");
    print(uid);
    print(", GID: ");
    println(gid);
    print("Size: ");
    println(size);
    println(perms);

    return 0;
}
