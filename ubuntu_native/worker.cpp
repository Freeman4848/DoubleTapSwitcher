#include "protocol.h"
#include "terminal_guard.h"
#include "../LiteSwitcheV1.1_source/DoubleTapDetector.h"
#include <systemd/sd-bus.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>
#include <glob.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
bool sendMessage(dt::Type type, int value = 0) {
    const dt::Message message{type, value};
    return send(STDIN_FILENO, &message, sizeof message, MSG_NOSIGNAL) == sizeof message;
}
bool activeUser(sd_bus *bus, uid_t uid) {
    sd_bus_message *reply = nullptr;
    if (sd_bus_call_method(bus, "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "ListSessions", nullptr, &reply, "") < 0) return false;
    bool found = false;
    if (sd_bus_message_enter_container(reply, 'a', "(susso)") > 0) {
        const char *id, *name, *seat, *path;
        std::uint32_t owner;
        while (sd_bus_message_read(reply, "(susso)", &id, &owner, &name, &seat, &path) > 0) {
            if (owner != uid || !seat || std::strcmp(seat, "seat0") != 0) continue;
            int active = 0, locked = 1, remote = 1;
            char *type = nullptr, *kind = nullptr;
            const char *service = "org.freedesktop.login1", *iface = "org.freedesktop.login1.Session";
            bool ok = sd_bus_get_property_trivial(bus, service, path, iface, "Active", nullptr, 'b', &active) >= 0 &&
                sd_bus_get_property_trivial(bus, service, path, iface, "LockedHint", nullptr, 'b', &locked) >= 0 &&
                sd_bus_get_property_trivial(bus, service, path, iface, "Remote", nullptr, 'b', &remote) >= 0 &&
                sd_bus_get_property_string(bus, service, path, iface, "Type", nullptr, &type) >= 0 &&
                sd_bus_get_property_string(bus, service, path, iface, "Class", nullptr, &kind) >= 0;
            found |= ok && active && !locked && !remote && std::strcmp(kind, "user") == 0 &&
                (std::strcmp(type, "wayland") == 0 || std::strcmp(type, "x11") == 0);
            free(type); free(kind);
        }
    }
    sd_bus_message_unref(reply);
    return found;
}
struct Worker {
    sd_bus *bus = nullptr;
    std::map<int, std::string> devices;
    std::set<std::string> known;
    ~Worker() { for (auto &d : devices) close(d.first); sd_bus_unref(bus); }
    void scan() {
        glob_t paths{};
        if (glob("/dev/input/by-path/*-event-kbd", 0, nullptr, &paths) == 0) {
            for (std::size_t i = 0; i < paths.gl_pathc; ++i) {
                char *resolved = realpath(paths.gl_pathv[i], nullptr);
                if (!resolved) continue;
                std::string path(resolved); free(resolved);
                if (known.count(path)) continue;
                int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (fd >= 0) { devices[fd] = path; known.insert(path); }
            }
        }
        globfree(&paths);
    }
};
int run() {
    ucred credentials{}; socklen_t length = sizeof credentials;
    if (getsockopt(0, SOL_SOCKET, SO_PEERCRED, &credentials, &length) < 0 || credentials.uid == 0)
        throw std::runtime_error("Desktop user socket required");
    Worker worker;
    if (sd_bus_open_system(&worker.bus) < 0 || !activeUser(worker.bus, credentials.uid)) {
        sendMessage(dt::Error, dt::SessionDenied);
        return 1;
    }
    sd_bus_set_method_call_timeout(worker.bus, 1000000);
    timeval timeout{3, 0}; setsockopt(0, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    dt::Config config{};
    std::size_t received = 0;
    while (received < sizeof config) {
        auto n = recv(0, reinterpret_cast<char *>(&config) + received, sizeof config - received, 0);
        if (n <= 0) throw std::runtime_error("Configuration timeout");
        received += static_cast<std::size_t>(n);
    }
    if (!dt::valid(config)) throw std::runtime_error("Invalid configuration");
    DoubleTapDetector detector;
    std::set<std::pair<int, unsigned>> modifiers;
    auto nextScan = Clock::now(), nextCheck = Clock::now();
    bool allowed = true, paused = false;
    worker.scan();
    if (!sendMessage(dt::Ready, static_cast<int>(worker.devices.size()))) return 0;
    for (;;) {
        auto now = Clock::now();
        if (now >= nextCheck) {
            bool current = activeUser(worker.bus, credentials.uid);
            if (!current || current != allowed) { detector.reset(); modifiers.clear(); }
            allowed = current; nextCheck = now + std::chrono::milliseconds(300);
        }
        if (now >= nextScan) {
            auto before = worker.devices.size();
            worker.scan(); nextScan = now + std::chrono::seconds(2);
            if (worker.devices.size() != before && !sendMessage(dt::Ready, static_cast<int>(worker.devices.size()))) return 0;
        }
        std::vector<pollfd> fds{{0, POLLIN, 0}};
        for (auto &device : worker.devices) fds.push_back({device.first, POLLIN, 0});
        if (poll(fds.data(), fds.size(), 100) < 0) return 0;
        if (fds[0].revents & (POLLHUP | POLLERR)) return 0;
        // Process physical input before a deletion request arriving in the same poll.
        for (std::size_t i = 1; i < fds.size(); ++i) {
            if (!fds[i].revents) continue;
            input_event events[64]{};
            auto bytes = read(fds[i].fd, events, sizeof events);
            if (bytes <= 0) {
                worker.known.erase(worker.devices[fds[i].fd]); worker.devices.erase(fds[i].fd);
                close(fds[i].fd); detector.reset(); modifiers.clear(); continue;
            }
            if (!allowed || (config.protect && dt::hiddenTerminal(credentials.uid))) {
                if (allowed && !paused) { if (!sendMessage(dt::Paused, 1)) return 0; paused = true; }
                detector.reset(); modifiers.clear(); continue;
            }
            for (std::size_t j = 0; j < static_cast<std::size_t>(bytes) / sizeof(input_event); ++j) {
                auto &e = events[j]; if (e.type != EV_KEY) continue;
                if (dt::modifier(e.code)) {
                    if (e.value == 0) modifiers.erase({fds[i].fd, e.code});
                    else modifiers.insert({fds[i].fd, e.code});
                }
                if (e.value == 0) { detector.key_up(e.code); continue; }

                if (e.value == 2) { detector.cancel(); continue; }
                int binding = -1;
                for (unsigned k = 0; k < config.count; ++k) if (config.keys[k] == e.code) binding = static_cast<int>(k);
                const std::uint64_t millis = static_cast<std::uint64_t>(e.time.tv_sec) * 1000 + e.time.tv_usec / 1000;
                if (detector.key_down(e.code, binding >= 0, !modifiers.empty(),
                    static_cast<std::uintptr_t>(fds[i].fd + 1), millis, config.threshold)) {
                    if (config.protect && dt::hiddenTerminal(credentials.uid, true)) {
                        detector.reset(); modifiers.clear();
                        if (!paused && !sendMessage(dt::Paused, 1)) return 0;
                        paused = true;
                        continue;
                    }
                    if (paused && !sendMessage(dt::Paused, 0)) return 0;
                    paused = false;
                    if (!sendMessage(dt::Target, binding)) return 0;
                }
            }
        }
        if (fds[0].revents & POLLIN) {
            char commands[64]{}; auto count = recv(0, commands, sizeof commands, 0);
            if (count <= 0) return 0;
            for (int i = 0; i < count; ++i) {
                if (commands[i] == 'R') { detector.reset(); modifiers.clear(); }
                else throw std::runtime_error("Unknown request");
            }
        }
    }
}
}
int main() {
    try { return run(); }
    catch (const std::exception &) {
        sendMessage(dt::Error, dt::InputUnavailable);
        return 1;
    }
}
