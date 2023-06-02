extern "C" {
    #include <unistd.h>     /* read and readlink syscalls */
    #include <fcntl.h>      /* open syscall */
    #include <dirent.h>     /* getdents64 syscall */
}

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unordered_map>

#define PROCFS_MOUNT        "/proc/"
#define BUF_SIZE            8192

namespace pstree {
    using children_map = std::unordered_map<pid_t, std::vector<pid_t>>;
}

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
 * Add a children pid to their parent process pid in a unordered hashmap
 * uniquely, so that they can later be retrieved for getting all children of
 * a process.
 */
void add_to_children_map (pstree::children_map &proc_children, pid_t ppid, pid_t child_pid) {
    auto entry = proc_children.find(ppid);

    if (entry != proc_children.end()) {
        // Get the child_pids vector of the pair
        auto &child_pids = entry->second;
        auto it = std::find(child_pids.begin(), child_pids.end(), child_pid);

        if (it == child_pids.end()) {
            child_pids.push_back(child_pid);
        }
    } else {
        proc_children[ppid] = {child_pid};
    }
}

/**
 * Returns a hash map that maps the pids of all processes to their parents' pid.
 */
pstree::children_map get_all_proc_children() {
    pstree::children_map proc_children;

    DIR *dir = opendir("/proc");
    dirent *entry = readdir(dir);

    // Go through every directory entry in the procfs
    while ((entry = readdir(dir)) != nullptr) {
        pid_t pid;

        // Only process entries that can be parsed as integers
        if (sscanf(entry->d_name, "%d", &pid) == 1) {
            std::istringstream stat_content_stream {get_proc_info_content(pid, "stat")};

            std::string tmp;
            pid_t ppid;

            // The parent's pid is in the fourth place of the stat pseudo-file
            stat_content_stream >> tmp >> tmp >> tmp >> ppid;

            // Add children pid to their parent process
            add_to_children_map(proc_children, ppid, pid);
        }
    }

    closedir(dir);

    return proc_children;
}

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
ProcInfoNode get_proc_tree(pid_t pid, pstree::children_map proc_children) {
    ProcInfoNode parent;

    parent.pid = pid;
    parent.name = get_proc_name(pid);

    // Recursively add children nodes
    for (const auto &child_pid : proc_children[pid]) {
        parent.children.push_back(get_proc_tree(child_pid, proc_children));
    }

    return parent;
}

/**
 * Prints the given node as a tree in JSON.
 */
void print_proc_tree(ProcInfoNode root) {
    std::cout << "[";

    std::cout << root.to_json();

    std::cout << "]" << std::endl;
}

/**
 * Prints the list of given nodes as a list in JSON.
 */
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
    auto proc_children = get_all_proc_children();

    if (argc > 1) {
        // If a valid process id is given, output the children of that one
        pid_t pid = strtoul(argv[1], nullptr, 10);

        if (!std::filesystem::is_directory("/proc/" + std::to_string(pid))) {
            std::cerr << "There is no process with the pid " << pid << std::endl;
            return -1;
        }

        ProcInfoNode root {get_proc_tree(pid, proc_children)};

        print_proc_tree(root);
    } else {
        // By default, output the children of the parent process #0
        std::vector<ProcInfoNode> proc_tree_list {get_proc_tree(0, proc_children).children};

        print_proc_tree_list(proc_tree_list);
    }

    return 0;
}
