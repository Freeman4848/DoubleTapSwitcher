#pragma once
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <glob.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

namespace dt {
enum class TerminalMode { Visible, Hidden, RawHidden };
inline TerminalMode terminalMode(int fd, uid_t uid, dev_t &device) {
    struct stat st{};
    termios mode{};
    if (fstat(fd, &st) != 0 || st.st_uid != uid || tcgetattr(fd, &mode) != 0 || (mode.c_lflag & ECHO))
        return TerminalMode::Visible;
    device = st.st_rdev;
    // sudo-rs hides input in noncanonical mode with ECHONL enabled.
    return mode.c_lflag & (ICANON | ECHONL) ? TerminalMode::Hidden : TerminalMode::RawHidden;
}

inline bool foregroundAuthentication(const std::string &stat, const std::set<dev_t> &terminals) {
    const auto begin = stat.find('('), end = stat.rfind(')');
    if (begin == std::string::npos || end == std::string::npos || end <= begin || end + 2 >= stat.size()) return false;
    const auto name = stat.substr(begin + 1, end - begin - 1);
    if (name != "sudo" && name != "sudo-rs" && name != "sudoedit" &&
        name != "su" && name != "su-rs" && name != "passwd") return false;
    char state = 0;
    long parent = 0, group = 0, session = 0, foreground = 0;
    long long encoded = 0;
    if (std::sscanf(stat.c_str() + end + 2, "%c %ld %ld %ld %lld %ld",
        &state, &parent, &group, &session, &encoded, &foreground) != 6) return false;
    if (state == 'Z' || state == 'T' || state == 't' || group <= 0 || foreground != group || encoded == 0) return false;
    const auto raw = static_cast<std::uint32_t>(encoded);
    const auto device = makedev((raw >> 8) & 0xfff, (raw & 0xff) | ((raw >> 12) & 0xfff00));
    return terminals.count(device) != 0;
}

// Read terminal flags and process metadata only, never input or command arguments.
// Raw authentication scanning is done only for a recognized double tap.
inline bool hiddenTerminal(uid_t uid, bool inspectRawAuthentication = false) {
    glob_t paths{};
    std::set<dev_t> rawTerminals;
    bool hidden = false;
    if (glob("/dev/pts/[0-9]*", 0, nullptr, &paths) == 0) {
        for (std::size_t i = 0; i < paths.gl_pathc; ++i) {
            int fd = open(paths.gl_pathv[i], O_RDONLY | O_NOCTTY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
            if (fd < 0) continue;
            dev_t device = 0;
            const auto mode = terminalMode(fd, uid, device);
            close(fd);
            if (mode == TerminalMode::Hidden) { hidden = true; break; }
            if (mode == TerminalMode::RawHidden) rawTerminals.insert(device);
        }
    }
    globfree(&paths);
    if (hidden || !inspectRawAuthentication || rawTerminals.empty()) return hidden;
    DIR *processes = opendir("/proc");
    if (!processes) return false;
    while (auto entry = readdir(processes)) {
        bool numeric = entry->d_name[0] != '\0';
        for (const char *c = entry->d_name; *c; ++c) numeric &= *c >= '0' && *c <= '9';
        if (!numeric) continue;
        const std::string path = std::string("/proc/") + entry->d_name + "/stat";
        FILE *file = std::fopen(path.c_str(), "r");
        if (!file) continue;
        char buffer[512]{};
        const auto length = std::fread(buffer, 1, sizeof buffer - 1, file);
        std::fclose(file);
        if (foregroundAuthentication(std::string(buffer, length), rawTerminals)) { hidden = true; break; }
    }
    closedir(processes);
    return hidden;
}
}
