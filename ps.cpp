extern "C" {
    #include <unistd.h>     /* read and readlink syscalls */
    #include <fcntl.h>      /* open syscall */
    #include <dirent.h>     /* getdents64 syscall */
}

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#define NAME_LIMIT          256
#define BUF_SIZE            8192

/**
 * Returns the path for a file under the /proc hierarchy.
 */
std::string get_proc_path(pid_t pid, const char name[]) {
    return "/proc/" + std::to_string(pid) + "/" + name;
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

    return std::string(buf, nread);
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
    std::string exe;
    std::string cwd;
    unsigned long long base_address;
    char state {'\0'};
    std::vector<std::string> cmdline;

    std::string to_json() {
        std::ostringstream json;

        json << "{";
        json << "\"pid\":" << pid << ",";
        json << "\"exe\":\"" << exe << "\",";
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
unsigned long long get_proc_base_address(pid_t pid, std::string exe) {
    std::string maps_content {get_proc_info_content(pid, "maps")};
    std::istringstream maps_content_stream (maps_content);

    std::string line;
    while (std::getline(maps_content_stream, line)) {
        std::istringstream linestream (line);
        std::string address, perms, offset, dev, inode, path;

        if (!(linestream >> address >> perms >> offset >> dev >> inode >> path)) {
            continue;
        }

        auto offset_num = strtoul(offset.c_str(), nullptr, 16);

        if (path == exe && offset_num == 0) {
            auto base_address = address.substr(0, address.find('-'));

            return strtoull(base_address.c_str(), nullptr, 16);
        }
    }

    return 0;
}

/**
 * Returns an array of the command line arguments as strings.
 */
std::vector<std::string> get_proc_cmdline(pid_t pid) {
    std::vector<std::string> cmdline_items;
    std::istringstream cmdline_content_stream (get_proc_info_content(pid, "cmdline"));

    std::string arg;

    while (std::getline(cmdline_content_stream, arg, '\0')) {
        std::istringstream arg_stream (arg);
        std::string tmp;

        while (std::getline(arg_stream, tmp, ' ')) {
            cmdline_items.push_back(arg);
        }
    }

    return cmdline_items;
}

/**
 * Returns the gathered information of the current processes running on the
 * system.
 */
std::vector<ProcInfo> get_proc_infos() {
    std::vector<ProcInfo> proc_infos;
    DIR *dir = opendir("/proc");
    dirent *entry = readdir(dir);

    // Go through every directory entry in the procfs
    while ((entry = readdir(dir)) != nullptr) {
        ProcInfo info;

        // Only process entries that can be parsed as integers
        if (sscanf(entry->d_name, "%d", &info.pid) == 1) {
            info.exe = get_proc_info_link(info.pid, "exe");
            if (info.exe.empty()) continue;

            info.cwd = get_proc_info_link(info.pid, "cwd");
            if (info.cwd.empty()) continue;

            info.base_address = get_proc_base_address(info.pid, info.exe);
            info.state = get_proc_state(info.pid);
            info.cmdline = get_proc_cmdline(info.pid);

            proc_infos.push_back(info);
        }
    }

    closedir(dir);

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

    std::cout << "]" << std::endl;
}

int main() {
    std::vector<ProcInfo> proc_infos {get_proc_infos()};

    print_proc_infos(proc_infos);

    return 0;
}
