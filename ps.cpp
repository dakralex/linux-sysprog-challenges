extern "C" {
    #include <unistd.h>     /* write syscall */
    #include <fcntl.h>      /* open syscall, AT_ constants */
    #include <dirent.h>     /* getdents64 syscall */
    #include <stdlib.h>     /* malloc, free, strtol */
    #include <stdio.h>      /* snprintf */
}

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#define PROCFS_MOUNT        "/proc/"
#define NAME_LIMIT          256
#define BUF_SIZE            4096
#define PATH_LIMIT          4096

/**
 * Returns the path for a file under the /proc hierarchy.
 */
std::string get_proc_path(pid_t pid, const char name[]) {
    char buf[NAME_LIMIT];
    ssize_t nput = std::snprintf(buf, NAME_LIMIT, "/proc/%d/%s", pid, name);
    buf[nput] = '\0';

    return buf;
}

/**
 * Returns the content of a file under the /proc hierarchy.
 */
std::string get_proc_info_content(pid_t pid, const char name[]) {
    std::string proc_info_path {get_proc_path(pid, name)};
    char buf[BUF_SIZE];

    int fd = open(proc_info_path.c_str(), O_RDONLY);
    if (fd == -1) return {};

    ssize_t nread = read(fd, buf, BUF_SIZE);
    if (nread == -1) return {};

    close(fd);
    buf[nread] = '\0';

    return buf;
}

/**
 * Returns the link of a file under the /proc hierarchy.
 */
std::string get_proc_info_link(pid_t pid, const char name[]) {
    std::string proc_info_path {get_proc_path(pid, name)};
    char buf[BUF_SIZE];

    ssize_t nread = readlink(proc_info_path.c_str(), buf, BUF_SIZE);
    if (nread == -1) return {};

    buf[nread] = '\0';

    return buf;
}

struct ProcInfo {
    pid_t pid;
    char state {'\0'};
    unsigned long long base_address;
    std::string exe;
    std::string cwd;
    std::vector<std::string> cmdline;

    std::string to_json() {
        std::ostringstream json;

        json << "{";
        json << "\"pid\":" << pid << ",";
        json << "\"exe:\":\"" << exe << "\",";
        json << "\"cwd\":\"" << cwd << "\",";
        json << "\"base_address\":" << base_address << ",";
        json << "\"state\":\"" << state << "\",";
        json << "\"cmdline\":" << "[";
        for (size_t i {0}; i < cmdline.size(); ++i) {
            json << "\"" << cmdline[i] << "\"";

            if (i != cmdline.size() - 1) {
                json << ",";
            }
        }
        json << "]";
        json << "}";

        return json.str();
    }
};

/**
 * Returns the state indicated by a char defined in `man 5 proc` from the
 * `/proc/pid/stat` file.
 */
char get_proc_state(pid_t pid) {
    std::string stat_content {get_proc_info_content(pid, "stat")};

    // Get the position of the stat char, which is two characters after a
    // closing parenthesis (see `man 5 proc`)
    auto stat_pos = stat_content.find(')') + 2;

    // Return the char two characters after the parenthesis
    return stat_content.at(stat_pos);
}

/**
 * Returns the base address which is the first address mapped to the process.
 */
unsigned long long get_proc_base_address(pid_t pid) {
    std::string maps_content {get_proc_info_content(pid, "maps")};

    auto first_address_end = maps_content.find('-') - 1;
    auto first_address = maps_content.substr(0, first_address_end);

    return strtoull(first_address.c_str(), nullptr, 16);
}

/**
 * Returns an array of the command line arguments as strings.
 */
std::vector<std::string> get_proc_cmdline(pid_t pid) {
    std::vector<std::string> cmdline_items;
    std::istringstream cmdline_content_stream (get_proc_info_content(pid, "cmdline"));

    std::string tmp;

    while (std::getline(cmdline_content_stream, tmp, ' ')) {
        cmdline_items.push_back(tmp);
    }

    return cmdline_items;
}

/**
 * Returns the gathered information of the current processes running on the
 * system.
 */
std::vector<ProcInfo> get_proc_infos() {
    std::vector<ProcInfo> proc_infos;
    int fd;
    char buf[BUF_SIZE];
    ssize_t nread;
    ssize_t bpos;
    dirent64 *procd {nullptr};
    ProcInfo info {};

    fd = open(PROCFS_MOUNT, O_RDONLY | O_DIRECTORY);

    do {
        nread = getdents64(fd, buf, BUF_SIZE);

        // stop if there is nothing left to read
        if (nread == 0) break;

        bpos = 0u;

        // go through all directory entries in procfs
        do {
            procd = (struct dirent64 *) (buf + bpos);

            // skip any entry that is not a folder with a number as its name
            if (procd->d_type != DT_DIR) break;
            if (procd->d_name[0] < '0' || procd->d_name[0] > '9') break;

            info.pid = strtoul(procd->d_name, nullptr, 10);
            info.state = get_proc_state(info.pid);
            info.base_address = get_proc_base_address(info.pid);
            info.exe = get_proc_info_link(info.pid, "exe");
            info.cwd = get_proc_info_link(info.pid, "cwd");
            info.cmdline = get_proc_cmdline(info.pid);

            proc_infos.push_back(info);

            // increment bpos
            bpos += procd->d_reclen;
        } while (bpos < nread);
    } while(nread > 0);

    close(fd);

    return proc_infos;
}

/**
 * Outputs the vector of ProcInfo as JSON to the standard output.
 */
void print_proc_infos(std::vector<ProcInfo> proc_infos) {
    std::cout << "[";

    for (size_t i {0}; i < proc_infos.size(); ++i) {
        std::cout << proc_infos[i].to_json();

        if (i != proc_infos.size() - 1) {
            std::cout << ",";
        }
    }

    std::cout << "]";
}

int main() {
    std::vector<ProcInfo> proc_infos {get_proc_infos()};

    print_proc_infos(proc_infos);

    return 0;
}
