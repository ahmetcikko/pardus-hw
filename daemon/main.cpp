#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <systemd/sd-login.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

static volatile sig_atomic_t g_stop = 0;

static void on_stop(int) { g_stop = 1; }
static std::string read_line(const std::string &path) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}
static bool write_file(const std::string &path, const std::string &value) {
    std::ofstream f(path);
    if (!f.is_open())
        return false;
    f << value;
    f.flush();
    return f.good();
}
static int config_int(const char *key, int def) {
    std::ifstream f("/etc/pardus-optimizer.conf");
    std::string line;
    std::string want = std::string(key) + "=";
    while (std::getline(f, line))
        if (line.rfind(want, 0) == 0)
            return std::atoi(line.c_str() + want.size());
    return def;
}
static bool run_cmd(const char *a, const char *b, const char *c,
                    const char *d) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {const_cast<char *>(a), const_cast<char *>(b),
                        const_cast<char *>(c), const_cast<char *>(d), nullptr};
        execvp(a, args);
        _exit(-1);
    }
    if (pid < 0)
        return false;
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
static void ensure_zram() {
    std::ifstream f("/proc/swaps");
    std::string line;
    while (std::getline(f, line))
        if (line.find("zram") != std::string::npos)
            return;
    run_cmd("modprobe", "zram", nullptr, nullptr);
    std::ifstream mi("/proc/meminfo");
    std::string key;
    std::uint64_t kb = 0;
    mi >> key >> kb;
    if (!kb)
        return;
    if (!write_file("/sys/block/zram0/disksize", std::to_string(kb * 512)))
        return;
    if (!run_cmd("mkswap", "/dev/zram0", nullptr, nullptr))
        return;
    run_cmd("swapon", "-p", "100", "/dev/zram0");
}
static std::string slice_path(uid_t uid) {
    return "/sys/fs/cgroup/user.slice/user-" + std::to_string(uid) + ".slice";
}
static void squeeze(uid_t uid) {
    std::string slice = slice_path(uid);
    int cpu = config_int("cpu", 20);
    int mem = config_int("mem", 40);
    write_file(slice + "/cpu.max", std::to_string(cpu * 1000) + " 100000");
    std::uint64_t current = std::strtoull(
        read_line(slice + "/memory.current").c_str(), nullptr, 10);
    std::uint64_t high = current * std::uint64_t(mem) / 100;
    if (high < 268435456ULL)
        high = 268435456ULL;
    write_file(slice + "/memory.high", std::to_string(high));
    if (config_int("freeze", 0))
        write_file(slice + "/cgroup.freeze", "1");
}
static void restore(uid_t uid) {
    std::string slice = slice_path(uid);
    write_file(slice + "/cgroup.freeze", "0");
    write_file(slice + "/cpu.max", "max 100000");
    write_file(slice + "/memory.high", "max");
}
static void reap_sessions(std::unordered_set<uid_t> &squeezed) {
    char **sessions = nullptr;
    int n = sd_get_sessions(&sessions);
    std::unordered_set<uid_t> seated;
    std::unordered_set<uid_t> active;
    for (int i = 0; i < n; i++) {
        uid_t uid;
        if (sd_session_get_uid(sessions[i], &uid) < 0)
            continue;
        char *seat = nullptr;
        if (sd_session_get_seat(sessions[i], &seat) < 0)
            continue;
        free(seat);
        seated.insert(uid);
        char *state = nullptr;
        if (sd_session_get_state(sessions[i], &state) >= 0) {
            if (std::string(state) == "active")
                active.insert(uid);
            free(state);
        }
    }
    if (sessions) {
        for (int i = 0; i < n; i++)
            free(sessions[i]);
        free(sessions);
    }
    for (uid_t uid : seated)
        if (uid >= 1000 && !active.count(uid) && !squeezed.count(uid)) {
            squeeze(uid);
            squeezed.insert(uid);
        }
    std::vector<uid_t> back;
    for (uid_t uid : squeezed)
        if (active.count(uid) || !seated.count(uid))
            back.push_back(uid);
    for (uid_t uid : back) {
        restore(uid);
        squeezed.erase(uid);
    }
}
static void optimize_sweep(std::unordered_set<uid_t> &squeezed) {
    reap_sessions(squeezed);
    sync();
    write_file("/proc/sys/vm/drop_caches", "3");
    write_file("/proc/sys/vm/compact_memory", "1");
}
static std::string proc_cgroup_path(int pid) {
    std::ifstream f("/proc/" + std::to_string(pid) + "/cgroup");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("0::", 0) == 0)
            return line.substr(3);
    return "";
}
static void shrink_process(int pid) {
    std::string origin = proc_cgroup_path(pid);
    if (origin.empty())
        return;
    std::ifstream statm("/proc/" + std::to_string(pid) + "/statm");
    std::uint64_t sz = 0, rss = 0;
    statm >> sz >> rss;
    rss *= std::uint64_t(sysconf(_SC_PAGESIZE));
    std::string dir = "/sys/fs/cgroup/pardushw-shrink";
    mkdir(dir.c_str(), 0755);
    if (!write_file(dir + "/cgroup.procs", std::to_string(pid)))
        return;
    std::uint64_t target = rss / 3;
    if (target < 8388608ULL)
        target = 8388608ULL;
    write_file(dir + "/memory.high", std::to_string(target));
    usleep(300000);
    write_file(dir + "/memory.high", "max");
    write_file("/sys/fs/cgroup" + origin + "/cgroup.procs", std::to_string(pid));
}
int main() {
    if (geteuid() != 0)
        return 1;
    int pid_fd =
        open("/run/pardus-optimizer.pid", O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (flock(pid_fd, LOCK_EX | LOCK_NB) == -1) {
        close(pid_fd);
        return 0;
    }
    signal(SIGTERM, on_stop);
    signal(SIGINT, on_stop);
    ensure_zram();
    unlink("/run/pardus-optimizer.trigger");
    mkfifo("/run/pardus-optimizer.trigger", 0666);
    chmod("/run/pardus-optimizer.trigger", 0666);
    int fifo_fd =
        open("/run/pardus-optimizer.trigger", O_RDWR | O_NONBLOCK | O_CLOEXEC);
    std::unordered_set<uid_t> squeezed;
    int interval_ms = config_int("interval_seconds", 600) * 1000;
    if (interval_ms <= 0)
        interval_ms = 600000;
    optimize_sweep(squeezed);
    while (!g_stop) {
        struct pollfd pfd;
        pfd.fd = fifo_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int r = poll(&pfd, 1, interval_ms);
        if (g_stop)
            break;
        if (r > 0 && (pfd.revents & POLLIN)) {
            std::string data;
            char buf[256];
            ssize_t n;
            while ((n = read(fifo_fd, buf, sizeof(buf))) > 0)
                data.append(buf, size_t(n));
            size_t pos = 0;
            bool did_optimize = false;
            while (pos < data.size()) {
                size_t nl = data.find('\n', pos);
                std::string cmd = data.substr(
                    pos, nl == std::string::npos ? std::string::npos : nl - pos);
                if (cmd.rfind("shrink ", 0) == 0)
                    shrink_process(std::atoi(cmd.c_str() + 7));
                else if (!did_optimize)
                    did_optimize = true;
                if (nl == std::string::npos)
                    break;
                pos = nl + 1;
            }
            if (did_optimize)
                optimize_sweep(squeezed);
        } else {
            optimize_sweep(squeezed);
        }
    }
    for (uid_t uid : squeezed)
        restore(uid);
    unlink("/run/pardus-optimizer.trigger");
    return 0;
}
