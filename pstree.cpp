extern "C" {
    #include <unistd.h>     /* read and readlink syscalls */
    #include <fcntl.h>      /* open syscall */
    #include <dirent.h>     /* getdents64 syscall */
}

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <filesystem>

#define PROCFS_MOUNT        "/proc/"
#define BUF_SIZE            8192

void build_proc_path(std::ostringstream &oss) {
    (void) oss; // ignore unused parameter
}

template<typename T, typename... Args>
void build_proc_path(std::ostringstream &oss, const T& value, const Args&... args) {
    oss << "/" << value;
    build_proc_path(oss, args...);
}

/**
 * Builds a path to the procfs with a variable amount of arguments.
 */
template<typename... Args>
std::string build_proc_path(pid_t pid, Args&... args) {
    std::ostringstream oss;

    oss << PROCFS_MOUNT << pid;
    build_proc_path(oss, args...);

    return oss.str();
}

/**
 * Returns the content of a file under the /proc hierarchy.
 */
template<typename... Args>
std::string get_proc_info_content(pid_t pid, Args&... args) {
    std::string proc_info_path {build_proc_path(pid, args...)};
    char buf[BUF_SIZE];

    int fd = open(proc_info_path.c_str(), O_RDONLY);
    if (fd == -1) return {};

    ssize_t nread = read(fd, buf, BUF_SIZE);
    if (nread == -1) return {};

    close(fd);
    buf[nread] = '\0';

    return std::string(buf, nread);
}

struct ProcInfoNode {
    pid_t pid;
    std::string name;
    std::vector<ProcInfoNode> children;

    std::string to_json() {
        std::ostringstream json;

        json << "{";
        json << "\"pid\":" << pid << ",";
        json << "\"name\":\"" << name << "\",";
        json << "\"children\":" << "[";
        for (size_t i {0}; i < children.size(); ++i) {
            json << children[i].to_json();

            if (i != children.size() - 1) {
                json << ",";
            }
        }
        json << "]";
        json << "}";

        return json.str();
    }
};

/**
 * Returns the processes' name as stated in the /proc/pid/comm pseudo-file.
 */
std::string get_proc_name(pid_t pid) {
    std::istringstream comm_content_stream {get_proc_info_content(pid, "comm")};
    std::string proc_name;

    std::getline(comm_content_stream, proc_name);

    return proc_name;
}

/**
 * Returns a ProcInfoNode that goes through the processes' children
 * recursively.
 */
ProcInfoNode get_proc_tree(pid_t pid) {
    ProcInfoNode parent;

    parent.pid = pid;
    parent.name = get_proc_name(pid);

    std::string pid_str = std::to_string(parent.pid);

    std::string children_pids = get_proc_info_content(pid, "task", pid_str, "children");

    if (!children_pids.empty()) {
        std::istringstream children_pid_stream (children_pids);
        std::string child_pid_str;

        while (std::getline(children_pid_stream, child_pid_str, ' ')) {
            pid_t child_pid {(pid_t) strtoul(child_pid_str.c_str(), nullptr, 10)};
            ProcInfoNode child {get_proc_tree(child_pid)};

            parent.children.push_back(child);
        }
    }

    return parent;
}

/**
 * Prints the given node as a tree as JSON.
 */
void print_proc_tree(ProcInfoNode root) {
    std::cout << "[";

    std::cout << root.to_json();

    std::cout << "]" << std::endl;
}

std::vector<ProcInfoNode> get_proc_tree_list() {
    std::vector<ProcInfoNode> proc_tree_list;
    DIR *dir = opendir("/proc");
    dirent *entry = readdir(dir);

    // Go through every directory entry in the procfs
    while ((entry = readdir(dir)) != nullptr) {
        ProcInfoNode root;
        pid_t pid;

        // Only process entries that can be parsed as integers
        if (sscanf(entry->d_name, "%d", &pid) == 1) {
            ProcInfoNode root {get_proc_tree(pid)};

            proc_tree_list.push_back(root);
        }
    }

    closedir(dir);

    return proc_tree_list;
}

void print_proc_tree_list(std::vector<ProcInfoNode> proc_tree_list) {
    std::cout << "[";

    for (size_t i {0}; i < proc_tree_list.size(); ++i) {
        std::cout << proc_tree_list[i].to_json();

        if (i != proc_tree_list.size() - 1) {
            std::cout << ",";
        }
    }

    std::cout << "]" << std::endl;
}

int main(int argc, const char *argv[]) {
    if (argc > 1) {
        pid_t pid = strtoul(argv[1], nullptr, 10);

        if (!std::filesystem::is_directory("/proc/" + std::to_string(pid))) {
            std::cerr << "There is no process with the pid " << pid << std::endl;
            return -1;
        }

        ProcInfoNode root {get_proc_tree(pid)};

        print_proc_tree(root);
    } else {
        std::vector<ProcInfoNode> proc_tree_list {get_proc_tree_list()};

        print_proc_tree_list(proc_tree_list);
    }

    return 0;
}
