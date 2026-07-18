#include "backend.h"
#include <QCoreApplication>
#include <QQmlEngine>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/limits.h>
#include <signal.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct NetDev {
    std::uint64_t rxb;
    std::uint64_t rxp;
    std::uint64_t rxe;
    std::uint64_t rxd;
    std::uint64_t txb;
    std::uint64_t txp;
    std::uint64_t txe;
    std::uint64_t txd;
};
static std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    if (a == std::string::npos)
        return "";
    size_t b = s.find_last_not_of(" \t\n\r");
    return s.substr(a, b - a + 1);
}
static std::string read_line(const std::string &path) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}
static std::string config_dir() {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    std::string base = xdg && *xdg ? std::string(xdg)
                                   : std::string(home ? home : "") + "/.config";
    return base + "/pardushw";
}
static double to_d(const std::string &s) {
    return std::strtod(s.c_str(), nullptr);
}
static std::uint64_t to_u(const std::string &s) {
    return std::strtoull(s.c_str(), nullptr, 10);
}
static std::vector<std::string> split(const std::string &s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}
static QVariantMap row(const QString &k, const QString &v) {
    QVariantMap r;
    r["k"] = k;
    r["v"] = v;
    return r;
}
static QVariantMap section(const QString &title, const QVariantList &rows) {
    QVariantMap s;
    s["title"] = title;
    s["rows"] = rows;
    return s;
}
static QString kb_text(std::uint64_t kb) {
    if (kb >= 1048576)
        return QString::number(kb / 1048576.0, 'f', 2) + " GB";
    return QString::number(kb / 1024.0, 'f', 1) + " MB";
}
static QString bytes_text(std::uint64_t b) {
    if (b >= 1073741824ULL)
        return QString::number(b / 1073741824.0, 'f', 1) + " GB";
    if (b >= 1048576ULL)
        return QString::number(b / 1048576.0, 'f', 1) + " MB";
    return QString::number(b / 1024.0, 'f', 1) + " KB";
}
static QString rate_text(double bps) {
    if (bps >= 1048576.0)
        return QString::number(bps / 1048576.0, 'f', 1) + " MB/s";
    return QString::number(qRound(bps / 1024.0)) + " KB/s";
}
static QString uptime_text() {
    double up = to_d(read_line("/proc/uptime"));
    int days = int(up) / 86400;
    int h = int(up) % 86400 / 3600;
    int m = int(up) % 3600 / 60;
    int s = int(up) % 60;
    return QString::asprintf("%dd %02d:%02d:%02d", days, h, m, s);
}
static void cpu_totals(std::uint64_t &total, std::uint64_t &idle) {
    std::ifstream f("/proc/stat");
    std::string cpu;
    std::uint64_t user = 0, nice = 0, sys = 0, idl = 0, iow = 0, irq = 0,
                  sirq = 0, steal = 0;
    f >> cpu >> user >> nice >> sys >> idl >> iow >> irq >> sirq >> steal;
    idle = idl + iow;
    total = user + nice + sys + idl + iow + irq + sirq + steal;
}
static QString freq_text() {
    std::string khz =
        read_line("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (!khz.empty())
        return QString::number(to_d(khz) / 1000000.0, 'f', 1) + " GHz";
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("cpu MHz", 0) == 0)
            return QString::number(
                       to_d(line.substr(line.find(':') + 1)) / 1000.0, 'f', 1) +
                   " GHz";
    return "";
}
static std::unordered_map<std::string, std::uint64_t> read_meminfo() {
    std::unordered_map<std::string, std::uint64_t> out;
    std::ifstream f("/proc/meminfo");
    std::string key, rest;
    std::uint64_t val;
    while (f >> key >> val) {
        std::getline(f, rest);
        key.pop_back();
        out[key] = val;
    }
    return out;
}
static std::unordered_map<std::string, std::string> cpuinfo_map() {
    std::unordered_map<std::string, std::string> out;
    std::ifstream f("/proc/cpuinfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty())
            break;
        size_t c = line.find(':');
        if (c == std::string::npos)
            continue;
        out[trim(line.substr(0, c))] = trim(line.substr(c + 1));
    }
    return out;
}
static std::unordered_map<std::string, std::uint64_t> stat_counters() {
    std::unordered_map<std::string, std::uint64_t> out;
    std::ifstream f("/proc/stat");
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string key;
        std::uint64_t v = 0;
        ss >> key >> v;
        out[key] = v;
    }
    return out;
}
static std::unordered_map<std::string, NetDev> read_netdev() {
    std::unordered_map<std::string, NetDev> out;
    std::ifstream f("/proc/net/dev");
    std::string line;
    std::getline(f, line);
    std::getline(f, line);
    while (std::getline(f, line)) {
        size_t c = line.find(':');
        if (c == std::string::npos)
            continue;
        std::string name = trim(line.substr(0, c));
        std::istringstream ss(line.substr(c + 1));
        NetDev d;
        std::uint64_t fifo, frame, comp, multi;
        ss >> d.rxb >> d.rxp >> d.rxe >> d.rxd >> fifo >> frame >> comp >>
            multi >> d.txb >> d.txp >> d.txe >> d.txd;
        out[name] = d;
    }
    return out;
}
static std::unordered_map<std::string, std::vector<std::uint64_t>>
read_diskstats() {
    std::unordered_map<std::string, std::vector<std::uint64_t>> out;
    std::ifstream f("/proc/diskstats");
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::uint64_t maj, mnr;
        std::string name;
        ss >> maj >> mnr >> name;
        std::vector<std::uint64_t> v;
        std::uint64_t x;
        while (ss >> x)
            v.push_back(x);
        out[name] = v;
    }
    return out;
}
static std::vector<std::string> nvidia_fields() {
    std::vector<std::string> out;
    FILE *p =
        popen("nvidia-smi "
              "--query-gpu=name,driver_version,utilization.gpu,memory.used,"
              "memory.total,temperature.gpu,power.draw,clocks.gr,clocks.mem,"
              "fan.speed --format=csv,noheader,nounits 2>/dev/null",
              "r");
    if (!p)
        return out;
    char buf[512];
    std::string line;
    if (fgets(buf, sizeof(buf), p))
        line = buf;
    if (pclose(p) != 0)
        return out;
    for (const std::string &field : split(line, ','))
        out.push_back(trim(field));
    if (out.size() < 10)
        out.clear();
    return out;
}
static std::string drm_card() {
    for (int i = 0; i < 8; i++) {
        std::string dev = "/sys/class/drm/card" + std::to_string(i) + "/device";
        std::error_code ec;
        if (std::filesystem::exists(dev + "/gpu_busy_percent", ec))
            return dev;
    }
    return "";
}
static QString gpu_tile() {
    std::vector<std::string> nv = nvidia_fields();
    if (!nv.empty())
        return QString::fromStdString(nv[2]) + "%\n" +
               QString::number(to_d(nv[3]) / 1024.0, 'f', 1) + "/" +
               QString::number(to_d(nv[4]) / 1024.0, 'f', 0) + "GB";
    std::string card = drm_card();
    if (!card.empty()) {
        QString busy =
            QString::fromStdString(trim(read_line(card + "/gpu_busy_percent")));
        std::uint64_t used = to_u(read_line(card + "/mem_info_vram_used"));
        std::uint64_t total = to_u(read_line(card + "/mem_info_vram_total"));
        if (total)
            return busy + "%\n" + QString::number(used / 1073741824.0, 'f', 1) +
                   "/" + QString::number(total / 1073741824.0, 'f', 0) + "GB";
        return busy + "%";
    }
    return "N/A";
}
Backend::Backend(QObject *parent)
    : QObject(parent), m_ncpu(sysconf(_SC_NPROCESSORS_ONLN)), m_prevtotal(0),
      m_previdle(0), m_engine(nullptr), m_optbefore(0) {
    prime();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Backend::poll);
    m_timer.start();
}
QString Backend::cpuText() const { return m_cputext; }
QString Backend::ramText() const { return m_ramtext; }
QString Backend::diskText() const { return m_disktext; }
QString Backend::gpuText() const { return m_gputext; }
QString Backend::netText() const { return m_nettext; }
QVariantList Backend::heavyTasks() const { return m_heavytasks; }
void Backend::prime() {
    cpu_totals(m_prevtotal, m_previdle);
    for (const auto &n : read_netdev())
        m_prevnet[n.first] = {n.second.rxb, n.second.txb};
}
void Backend::poll() {
    std::uint64_t total, idle;
    cpu_totals(total, idle);
    std::uint64_t dtotal = total - m_prevtotal;
    std::uint64_t didle = idle - m_previdle;
    double cpu = dtotal ? 100.0 * (dtotal - didle) / dtotal : 0.0;
    m_prevtotal = total;
    m_previdle = idle;
    m_cputext = QString::number(qRound(cpu)) + "%\n" + freq_text();
    auto mem = read_meminfo();
    std::uint64_t mt = mem["MemTotal"];
    std::uint64_t mu = mt - mem["MemAvailable"];
    double mp = mt ? 100.0 * mu / mt : 0.0;
    m_ramtext = QString::number(qRound(mp)) + "%\n" +
                QString::number(mu / 1048576.0, 'f', 1) + "/" +
                QString::number(mt / 1048576.0, 'f', 0) + "GB";
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        std::uint64_t dt = std::uint64_t(vfs.f_blocks) * vfs.f_frsize;
        std::uint64_t du = dt - std::uint64_t(vfs.f_bfree) * vfs.f_frsize;
        double dp = dt ? 100.0 * du / dt : 0.0;
        m_disktext = QString::number(qRound(dp)) + "%\n" +
                     QString::number(du >> 30) + "/" +
                     QString::number(dt >> 30) + "GB";
    }
    m_gputext = gpu_tile();
    auto net = read_netdev();
    double rxrate = 0.0, txrate = 0.0;
    for (const auto &n : net) {
        auto pit = m_prevnet.find(n.first);
        double rr = 0.0, tr = 0.0;
        if (pit != m_prevnet.end() && n.second.rxb >= pit->second.first) {
            rr = double(n.second.rxb - pit->second.first);
            tr = double(n.second.txb - pit->second.second);
        }
        m_ifrates[n.first] = {rr, tr};
        if (n.first != "lo") {
            rxrate += rr;
            txrate += tr;
        }
        m_prevnet[n.first] = {n.second.rxb, n.second.txb};
    }
    m_nettext = "↓ " + rate_text(rxrate) + "\n↑ " + rate_text(txrate);
    std::vector<std::pair<int, std::uint64_t>> rsslist;
    long page = sysconf(_SC_PAGESIZE);
    std::error_code ec;
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator("/proc", ec)) {
        std::string name = entry.path().filename().string();
        if (name.find_first_not_of("0123456789") != std::string::npos)
            continue;
        std::ifstream cmd("/proc/" + name + "/cmdline");
        std::string arg0;
        std::getline(cmd, arg0, '\0');
        if (arg0.empty())
            continue;
        std::ifstream sm("/proc/" + name + "/statm");
        std::uint64_t sz = 0, rss = 0;
        sm >> sz >> rss;
        if (rss)
            rsslist.push_back({std::stoi(name), rss});
    }
    std::sort(rsslist.begin(), rsslist.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    QVariantList heavy;
    for (size_t i = 0; i < rsslist.size() && i < 5; i++) {
        int pid = rsslist[i].first;
        std::uint64_t bytes = rsslist[i].second * std::uint64_t(page);
        QVariantMap m;
        m["pid"] = pid;
        m["name"] = QString::fromStdString(
            trim(read_line("/proc/" + std::to_string(pid) + "/comm")));
        m["mem"] = int(bytes / 1048576);
        m["pct"] = mt ? qRound(100.0 * bytes / (mt * 1024.0)) : 0;
        heavy.append(m);
    }
    m_heavytasks = heavy;
    emit statsChanged();
}
QVariantList Backend::details(const QString &kind) {
    if (kind == "CPU")
        return cpu_details();
    if (kind == "RAM")
        return ram_details();
    if (kind == "Disk")
        return disk_details();
    if (kind == "GPU")
        return gpu_details();
    return net_details();
}
QVariantList Backend::cpu_details() {
    QVariantList out;
    auto info = cpuinfo_map();
    struct utsname un;
    uname(&un);
    QVariantList proc;
    proc.append(row(tr("Name"), QString::fromStdString(info["model name"])));
    proc.append(row(tr("Vendor"), QString::fromStdString(info["vendor_id"])));
    proc.append(row(tr("Architecture"), un.machine));
    proc.append(row(tr("Cores"), QString::fromStdString(info["cpu cores"])));
    proc.append(row(tr("Threads"), QString::number(m_ncpu)));
    proc.append(
        row(tr("Family / Model / Stepping"),
            QString::fromStdString(info["cpu family"] + " / " + info["model"] +
                                   " / " + info["stepping"])));
    proc.append(
        row(tr("Microcode"), QString::fromStdString(info["microcode"])));
    proc.append(row(tr("Address sizes"),
                    QString::fromStdString(info["address sizes"])));
    proc.append(row(tr("BogoMIPS"), QString::fromStdString(info["bogomips"])));
    out.append(section(tr("Processor"), proc));
    QVariantList clocks;
    clocks.append(row(tr("Current"), freq_text()));
    std::string mn =
        read_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
    if (!mn.empty())
        clocks.append(row(tr("Minimum"),
                          QString::number(to_d(mn) / 1000.0, 'f', 0) + " MHz"));
    std::string mx =
        read_line("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (!mx.empty())
        clocks.append(row(tr("Maximum"),
                          QString::number(to_d(mx) / 1000.0, 'f', 0) + " MHz"));
    clocks.append(
        row(tr("Governor"),
            QString::fromStdString(read_line(
                "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"))));
    clocks.append(
        row(tr("Driver"),
            QString::fromStdString(read_line(
                "/sys/devices/system/cpu/cpu0/cpufreq/scaling_driver"))));
    out.append(section(tr("Clocks"), clocks));
    QVariantList cache;
    for (int i = 0; i < 10; i++) {
        std::string base =
            "/sys/devices/system/cpu/cpu0/cache/index" + std::to_string(i);
        std::error_code ec;
        if (!std::filesystem::exists(base, ec))
            break;
        std::string type = read_line(base + "/type");
        QString label =
            "L" + QString::fromStdString(read_line(base + "/level"));
        if (type == "Data")
            label += " " + tr("Data");
        else if (type == "Instruction")
            label += " " + tr("Instruction");
        cache.append(row(
            label, QString::fromStdString(read_line(base + "/size")) + ", " +
                       QString::fromStdString(
                           read_line(base + "/ways_of_associativity")) +
                       "-way"));
    }
    out.append(section(tr("Cache"), cache));
    QVariantList sys;
    std::vector<std::string> la = split(read_line("/proc/loadavg"), ' ');
    if (la.size() >= 4) {
        sys.append(
            row(tr("Load average"),
                QString::fromStdString(la[0] + "  " + la[1] + "  " + la[2])));
        sys.append(
            row(tr("Tasks (running/total)"), QString::fromStdString(la[3])));
    }
    sys.append(row(tr("Uptime"), uptime_text()));
    auto counters = stat_counters();
    sys.append(row(tr("Context switches"), QString::number(counters["ctxt"])));
    sys.append(row(tr("Interrupts"), QString::number(counters["intr"])));
    sys.append(
        row(tr("Forks since boot"), QString::number(counters["processes"])));
    sys.append(row(tr("Kernel"), un.release));
    out.append(section(tr("System"), sys));
    return out;
}
QVariantList Backend::ram_details() {
    QVariantList out;
    auto mem = read_meminfo();
    std::uint64_t mt = mem["MemTotal"];
    std::uint64_t mu = mt - mem["MemAvailable"];
    QVariantList main;
    main.append(row(tr("Total"), kb_text(mt)));
    main.append(row(tr("Used"), kb_text(mu)));
    main.append(row(tr("Available"), kb_text(mem["MemAvailable"])));
    main.append(row(tr("Free"), kb_text(mem["MemFree"])));
    main.append(row(tr("Usage"),
                    QString::number(mt ? qRound(100.0 * mu / mt) : 0) + "%"));
    out.append(section(tr("Memory"), main));
    QVariantList det;
    det.append(row(tr("Buffers"), kb_text(mem["Buffers"])));
    det.append(row(tr("Cached"), kb_text(mem["Cached"])));
    det.append(row(tr("Shared"), kb_text(mem["Shmem"])));
    det.append(row(tr("Slab"), kb_text(mem["Slab"])));
    det.append(row(tr("Dirty"), kb_text(mem["Dirty"])));
    det.append(row(tr("Active"), kb_text(mem["Active"])));
    det.append(row(tr("Inactive"), kb_text(mem["Inactive"])));
    det.append(row(tr("Page tables"), kb_text(mem["PageTables"])));
    det.append(row(tr("Mapped"), kb_text(mem["Mapped"])));
    out.append(section(tr("Breakdown"), det));
    QVariantList swap;
    swap.append(row(tr("Total"), kb_text(mem["SwapTotal"])));
    swap.append(row(tr("Used"), kb_text(mem["SwapTotal"] - mem["SwapFree"])));
    swap.append(row(tr("Free"), kb_text(mem["SwapFree"])));
    swap.append(row(tr("Swappiness"), QString::fromStdString(read_line(
                                          "/proc/sys/vm/swappiness"))));
    out.append(section(tr("Swap"), swap));
    QVariantList virt;
    virt.append(row(tr("Commit limit"), kb_text(mem["CommitLimit"])));
    virt.append(row(tr("Committed"), kb_text(mem["Committed_AS"])));
    virt.append(row(tr("Vmalloc used"), kb_text(mem["VmallocUsed"])));
    virt.append(row(tr("HugePages"), QString::number(mem["HugePages_Total"]) +
                                         " × " + kb_text(mem["Hugepagesize"])));
    out.append(section(tr("Virtual"), virt));
    return out;
}
QVariantList Backend::disk_details() {
    QVariantList out;
    auto stats = read_diskstats();
    std::vector<std::string> names;
    std::error_code ec;
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator("/sys/block", ec)) {
        std::string name = entry.path().filename().string();
        if (name.rfind("loop", 0) == 0 || name.rfind("ram", 0) == 0 ||
            name.rfind("zram", 0) == 0 || name.rfind("sr", 0) == 0 ||
            name.rfind("fd", 0) == 0)
            continue;
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    for (const std::string &name : names) {
        QVariantList rows;
        std::string model =
            trim(read_line("/sys/block/" + name + "/device/model"));
        if (!model.empty())
            rows.append(row(tr("Model"), QString::fromStdString(model)));
        rows.append(row(
            tr("Size"),
            bytes_text(to_u(read_line("/sys/block/" + name + "/size")) * 512)));
        rows.append(row(tr("Type"), read_line("/sys/block/" + name +
                                              "/queue/rotational") == "0"
                                        ? "SSD"
                                        : "HDD"));
        auto it = stats.find(name);
        if (it != stats.end() && it->second.size() >= 10) {
            rows.append(row(tr("Reads"), QString::number(it->second[0])));
            rows.append(row(tr("Data read"), bytes_text(it->second[2] * 512)));
            rows.append(row(tr("Writes"), QString::number(it->second[4])));
            rows.append(
                row(tr("Data written"), bytes_text(it->second[6] * 512)));
            rows.append(
                row(tr("Busy time"),
                    QString::number(it->second[9] / 1000.0, 'f', 0) + " s"));
        }
        out.append(section(QString::fromStdString(name), rows));
    }
    QVariantList fs;
    std::ifstream mounts("/proc/mounts");
    std::string line;
    std::vector<std::string> seen;
    while (std::getline(mounts, line)) {
        std::vector<std::string> parts = split(line, ' ');
        if (parts.size() < 3 || parts[0].rfind("/dev/", 0) != 0)
            continue;
        if (std::find(seen.begin(), seen.end(), parts[0]) != seen.end())
            continue;
        seen.push_back(parts[0]);
        struct statvfs vfs;
        if (statvfs(parts[1].c_str(), &vfs) != 0)
            continue;
        std::uint64_t total = std::uint64_t(vfs.f_blocks) * vfs.f_frsize;
        std::uint64_t used = total - std::uint64_t(vfs.f_bfree) * vfs.f_frsize;
        if (!total)
            continue;
        fs.append(row(QString::fromStdString(parts[1]),
                      QString::fromStdString(parts[2]) + ", " +
                          QString::number(used >> 30) + "/" +
                          QString::number(total >> 30) + " GB (" +
                          QString::number(qRound(100.0 * used / total)) +
                          "%)"));
    }
    out.append(section(tr("Filesystems"), fs));
    return out;
}
QVariantList Backend::gpu_details() {
    QVariantList out;
    std::vector<std::string> nv = nvidia_fields();
    if (!nv.empty()) {
        QVariantList adapter;
        adapter.append(row(tr("Name"), QString::fromStdString(nv[0])));
        adapter.append(row(tr("Driver"), QString::fromStdString(nv[1])));
        out.append(section(tr("Adapter"), adapter));
        QVariantList status;
        status.append(
            row(tr("Utilization"), QString::fromStdString(nv[2]) + " %"));
        status.append(row(tr("Memory"), QString::fromStdString(nv[3]) + " / " +
                                            QString::fromStdString(nv[4]) +
                                            " MiB"));
        status.append(
            row(tr("Temperature"), QString::fromStdString(nv[5]) + " °C"));
        status.append(
            row(tr("Power draw"), QString::fromStdString(nv[6]) + " W"));
        status.append(
            row(tr("Fan speed"), QString::fromStdString(nv[9]) + " %"));
        out.append(section(tr("Status"), status));
        QVariantList clocks;
        clocks.append(
            row(tr("Graphics"), QString::fromStdString(nv[7]) + " MHz"));
        clocks.append(
            row(tr("Memory"), QString::fromStdString(nv[8]) + " MHz"));
        out.append(section(tr("Clocks"), clocks));
        return out;
    }
    std::string card = drm_card();
    if (!card.empty()) {
        QVariantList adapter;
        std::ifstream ue(card + "/uevent");
        std::string line;
        while (std::getline(ue, line)) {
            if (line.rfind("DRIVER=", 0) == 0)
                adapter.append(
                    row(tr("Driver"), QString::fromStdString(line.substr(7))));
            if (line.rfind("PCI_ID=", 0) == 0)
                adapter.append(
                    row(tr("PCI ID"), QString::fromStdString(line.substr(7))));
        }
        out.append(section(tr("Adapter"), adapter));
        QVariantList status;
        status.append(row(tr("Utilization"),
                          QString::fromStdString(
                              trim(read_line(card + "/gpu_busy_percent"))) +
                              " %"));
        std::uint64_t used = to_u(read_line(card + "/mem_info_vram_used"));
        std::uint64_t total = to_u(read_line(card + "/mem_info_vram_total"));
        if (total)
            status.append(
                row(tr("VRAM"), bytes_text(used) + " / " + bytes_text(total)));
        std::error_code ec;
        for (const std::filesystem::directory_entry &entry :
             std::filesystem::directory_iterator(card + "/hwmon", ec)) {
            std::string t = read_line(entry.path().string() + "/temp1_input");
            if (!t.empty()) {
                status.append(
                    row(tr("Temperature"),
                        QString::number(to_d(t) / 1000.0, 'f', 0) + " °C"));
                break;
            }
        }
        out.append(section(tr("Status"), status));
        return out;
    }
    QVariantList none;
    none.append(row(tr("Status"), tr("Unavailable")));
    out.append(section(tr("GPU"), none));
    return out;
}
QVariantList Backend::net_details() {
    QVariantList out;
    auto net = read_netdev();
    std::vector<std::string> names;
    for (const auto &n : net)
        if (n.first != "lo")
            names.push_back(n.first);
    std::sort(names.begin(), names.end());
    for (const std::string &name : names) {
        const NetDev &d = net[name];
        QVariantList rows;
        rows.append(
            row(tr("State"), QString::fromStdString(read_line(
                                 "/sys/class/net/" + name + "/operstate"))));
        std::string speed = read_line("/sys/class/net/" + name + "/speed");
        rows.append(
            row(tr("Speed"), speed.empty() || speed[0] == '-'
                                 ? tr("Unknown")
                                 : QString::fromStdString(speed) + " Mb/s"));
        rows.append(row(tr("MTU"), QString::fromStdString(read_line(
                                       "/sys/class/net/" + name + "/mtu"))));
        rows.append(row(tr("MAC"),
                        QString::fromStdString(
                            read_line("/sys/class/net/" + name + "/address"))));
        auto rit = m_ifrates.find(name);
        if (rit != m_ifrates.end()) {
            rows.append(row(tr("Download"), rate_text(rit->second.first)));
            rows.append(row(tr("Upload"), rate_text(rit->second.second)));
        }
        rows.append(row(tr("RX data"), bytes_text(d.rxb)));
        rows.append(row(tr("TX data"), bytes_text(d.txb)));
        rows.append(row(tr("RX packets"), QString::number(d.rxp)));
        rows.append(row(tr("TX packets"), QString::number(d.txp)));
        rows.append(
            row(tr("RX errors / dropped"),
                QString::number(d.rxe) + " / " + QString::number(d.rxd)));
        rows.append(
            row(tr("TX errors / dropped"),
                QString::number(d.txe) + " / " + QString::number(d.txd)));
        out.append(section(QString::fromStdString(name), rows));
    }
    return out;
}
void Backend::set_engine(QQmlEngine *engine) {
    m_engine = engine;
    std::ifstream f(config_dir() + "/config");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("lang=", 0) == 0 && line.substr(5) == "tr" &&
            m_translator.load(":/i18n/hwcenter_tr.qm"))
            QCoreApplication::installTranslator(&m_translator);
}
void Backend::setLanguage(const QString &lang) {
    if (lang == "tr") {
        if (m_translator.isEmpty() &&
            !m_translator.load(":/i18n/hwcenter_tr.qm"))
            return;
        QCoreApplication::installTranslator(&m_translator);
    } else {
        QCoreApplication::removeTranslator(&m_translator);
    }
    std::error_code ec;
    std::filesystem::create_directories(config_dir(), ec);
    std::ofstream f(config_dir() + "/config", std::ios::trunc);
    f << "lang=" << lang.toStdString() << "\n";
    if (m_engine)
        m_engine->retranslate();
}
void Backend::startOptimize() {
    auto mem = read_meminfo();
    m_optbefore = mem["MemFree"];
    int fd = open("/run/pardus-optimizer.trigger", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        emit optimizeDone(false, 0);
        return;
    }
    bool ok = write(fd, "1", 1) == 1;
    close(fd);
    if (!ok) {
        emit optimizeDone(false, 0);
        return;
    }
    QTimer::singleShot(3000, this, [this]() {
        auto after = read_meminfo();
        std::int64_t freed =
            std::int64_t(after["MemFree"]) - std::int64_t(m_optbefore);
        if (freed < 0)
            freed = 0;
        emit optimizeDone(true, int(freed / 1024));
    });
}
bool Backend::killProcess(int pid) { return kill(pid, SIGTERM) == 0; }
bool Backend::throttleProcess(int pid) {
    return setpriority(PRIO_PROCESS, pid, 19) == 0;
}
bool Backend::revealProcess(int pid) {
    char buf[PATH_MAX];
    std::string link = "/proc/" + std::to_string(pid) + "/exe";
    ssize_t len = readlink(link.c_str(), buf, sizeof(buf) - 1);
    if (len <= 0)
        return false;
    buf[len] = '\0';
    std::string dir = std::filesystem::path(buf).parent_path().string();
    pid_t child = fork();
    if (child == 0) {
        char *args[] = {const_cast<char *>("xdg-open"),
                        const_cast<char *>(dir.c_str()), nullptr};
        execvp("xdg-open", args);
        _exit(-1);
    }
    if (child < 0)
        return false;
    std::thread([child]() { waitpid(child, nullptr, 0); }).detach();
    return true;
}
