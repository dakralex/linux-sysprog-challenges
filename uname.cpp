extern "C" {
    #include <unistd.h>         /* write syscall */
    #include <sys/utsname.h>    /* uname syscall, utsname struct */
}

#define FD_STDOUT       1
#define FD_STDERR       2

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

int main() {
    struct utsname unameData;

    if (uname(&unameData) != 0) {
        println("Error: Syscall uname() failed", FD_STDERR);

        return -1;
    }

    print("Hostname: ");
    println(unameData.nodename);
    print("OS: ");
    println(unameData.sysname);
    print("Version: ");
    println(unameData.version);
    print("Release: ");
    println(unameData.release);

    return 0;
}
