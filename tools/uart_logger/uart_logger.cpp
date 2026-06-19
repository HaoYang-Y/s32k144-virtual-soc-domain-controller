/**
 * @file    uart_logger.cpp
 * @brief   S32K144 UART 日志采集工具 —— 接收 MCU 串口输出，落盘+终端显示。
 *
 * 用法:
 *   ./tools/uart_logger                    前台运行（终端输出 + 写文件）
 *   ./tools/uart_logger &                  后台运行（仅写文件）
 *   ./tools/uart_logger /dev/ttyUSB1 460800
 *
 * 日志文件:  tools/log/uart_YYYYMMDD_HHMMSS.log
 *
 * 编译:     g++ -std=c++17 -O2 -Wall -o tools/uart_logger tools/uart_logger.cpp
 */

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

// =========================================================================
// 默认配置
// =========================================================================
static constexpr const char *kDefaultPort = "/dev/ttyUSB0";
static constexpr int         kDefaultBaud = 115200;
static constexpr const char *kLogSubdir  = "log";
static constexpr const char *kLogPrefix  = "uart";

// =========================================================================
// 全局 —— 用于信号安全退出
// =========================================================================
static std::atomic<bool> g_running{true};

extern "C" void handle_signal(int /*sig*/) {
    g_running.store(false, std::memory_order_release);
}

// =========================================================================
// 工具函数
// =========================================================================

/** 获取 tools/ 所在目录的绝对路径 */
static std::string self_dir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n == -1) return ".";
    buf[n] = '\0';
    std::string path(buf);
    auto pos = path.rfind('/');
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

/** 确保目录存在 */
static bool ensure_dir(const std::string &dir) {
    struct stat sb{};
    if (::stat(dir.c_str(), &sb) == 0) return S_ISDIR(sb.st_mode);
    return ::mkdir(dir.c_str(), 0755) == 0;
}

/** 生成日志文件路径: tools/log/uart_20260619_143021.log */
static std::string make_log_path() {
    auto now   = std::time(nullptr);
    auto *tm   = std::localtime(&now);
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tm);

    std::string dir = self_dir() + "/" + kLogSubdir;
    ensure_dir(dir);
    return dir + "/" + kLogPrefix + "_" + ts + ".log";
}

/** 当前时间戳字符串 "[2026-06-19 14:30:21.123]" */
static std::string now_str() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t   = system_clock::to_time_t(now);
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    // 拼接毫秒
    snprintf(buf + 19, sizeof(buf) - 19, ".%03ld", ms.count());
    return std::string("[") + buf + "]";
}

/** 检测是否前台: stdout 是终端 → true; 被管道/后台 → false */
static bool is_foreground() {
    return isatty(STDOUT_FILENO) == 1;
}

// =========================================================================
// 串口操作 (POSIX termios, 零依赖)
// =========================================================================

static speed_t baud_to_speed(int baud) {
    switch (baud) {
    case 9600:    return B9600;
    case 19200:   return B19200;
    case 38400:   return B38400;
    case 57600:   return B57600;
    case 115200:  return B115200;
    case 230400:  return B230400;
    case 460800:  return B460800;
    case 921600:  return B921600;
    default:      return B115200;
    }
}

/** 打开并配置串口，返回 fd，失败返回 -1 */
static int serial_open(const std::string &port, int baud) {
    int fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        std::cerr << "[ERROR] open(" << port << "): " << std::strerror(errno) << "\n";
        return -1;
    }

    struct termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "[ERROR] tcgetattr: " << std::strerror(errno) << "\n";
        close(fd);
        return -1;
    }

    // 原始模式，8N1，无流控
    cfmakeraw(&tty);
    tty.c_cflag &= ~PARENB;              // 无校验
    tty.c_cflag &= ~CSTOPB;              // 1 停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;                  // 8 数据位
    tty.c_cflag &= ~CRTSCTS;             // 无硬件流控
    tty.c_cflag |= CREAD | CLOCAL;       // 开启接收，忽略调制解调器控制线

    tty.c_cc[VMIN]  = 0;                // 非阻塞读
    tty.c_cc[VTIME] = 10;               // 1.0 秒超时 (0.1s * 10)

    cfsetispeed(&tty, baud_to_speed(baud));
    cfsetospeed(&tty, baud_to_speed(baud));

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "[ERROR] tcsetattr: " << std::strerror(errno) << "\n";
        close(fd);
        return -1;
    }

    // 清空缓冲区
    tcflush(fd, TCIOFLUSH);

    // 切回阻塞模式便于 readline
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return fd;
}

// =========================================================================
// 日志写入器
// =========================================================================

class LogWriter {
public:
    explicit LogWriter(const std::string &path) : path_(path) {}

    void write(const std::string &line) {
        if (!file_.is_open()) {
            file_.open(path_, std::ios::out | std::ios::app);
            if (!file_) {
                std::cerr << "[WARN] 无法打开日志文件: " << path_ << "\n";
                return;
            }
        }
        file_ << line << "\n";
        file_.flush();
    }

    const std::string &path() const { return path_; }

private:
    std::string   path_;
    std::ofstream file_;
};

// =========================================================================
// 逐字节读串口，拼成行
// =========================================================================

static std::string serial_readline(int fd) {
    std::string line;
    char        ch;
    while (g_running.load(std::memory_order_acquire)) {
        ssize_t n = read(fd, &ch, 1);
        if (n == 0) {
            // 超时，返回空让上层继续
            return {};
        }
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            return {};  // 错误
        }
        if (ch == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }
        if (ch != '\r') {
            line += ch;
        }
    }
    return {};
}

// =========================================================================
// 入口
// =========================================================================

int main(int argc, char *argv[]) {
    // ---- 解析参数 ----
    std::string port = kDefaultPort;
    int         baud = kDefaultBaud;

    if (argc >= 2) port = argv[1];
    if (argc >= 3) baud = std::stoi(argv[2]);

    // ---- 前台/后台检测 ----
    bool foreground = is_foreground();

    // ---- 安装信号 ----
    std::signal(SIGINT,  handle_signal);
    std::signal(SIGTERM, handle_signal);
    std::signal(SIGPIPE, SIG_IGN);

    // ---- 打开串口 ----
    int fd = serial_open(port, baud);
    if (fd == -1) return 1;

    // ---- 创建日志文件 ----
    LogWriter log(make_log_path());

    // ---- 启动信息 ----
    std::string mode_str = foreground ? "前台 (终端+文件)" : "后台 (仅文件)";
    std::string banner = "[uart_logger] " + port + " @ " + std::to_string(baud) +
                         " bps  |  模式: " + mode_str;
    std::cerr << banner << "\n";
    std::cerr << "[uart_logger] 日志文件: " << log.path() << "\n";

    if (foreground) {
        std::cerr << "[uart_logger] 按 Ctrl-C 停止\n\n";
    }

    // 写文件头
    auto now = std::time(nullptr);
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    log.write("=== UART Logger started " + std::string(time_buf) + " ===");
    log.write("=== port=" + port + " baud=" + std::to_string(baud) + " ===");
    log.write("");

    // ---- 主循环 ----
    while (g_running.load(std::memory_order_acquire)) {
        std::string line = serial_readline(fd);
        if (line.empty()) continue;

        std::string decorated = now_str() + " " + line;

        if (foreground) {
            std::cout << decorated << "\n" << std::flush;
        }
        log.write(decorated);
    }

    // ---- 清理 ----
    log.write("");
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    log.write("=== UART Logger stopped " + std::string(time_buf) + " ===");

    close(fd);
    std::cerr << "\n[uart_logger] 已停止，日志保存在: " << log.path() << "\n";

    return 0;
}
