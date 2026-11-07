#include "protocol.h"
#include "terminal_guard.h"
#include "../LiteSwitcheV1.1_source/DoubleTapDetector.h"
#include <cassert>
#include <cstdio>
#include <pty.h>

void terminalTests() {
    int master=-1, slave=-1;
    assert(openpty(&master,&slave,nullptr,nullptr,nullptr)==0);
    termios flags{};
    assert(tcgetattr(slave,&flags)==0);
    flags.c_lflag |= ECHO | ICANON;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    dev_t device=0;
    assert(dt::terminalMode(slave,getuid(),device)==dt::TerminalMode::Visible);
    flags.c_lflag &= ~ECHO;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    assert(dt::terminalMode(slave,getuid(),device)==dt::TerminalMode::Hidden);
    flags.c_lflag &= ~ICANON;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    assert(dt::terminalMode(slave,getuid(),device)==dt::TerminalMode::RawHidden);
    flags.c_lflag |= ECHONL;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    assert(dt::terminalMode(slave,getuid(),device)==dt::TerminalMode::Hidden);
    flags.c_lflag &= ~ECHONL;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    assert(dt::terminalMode(slave,getuid()+1,device)==dt::TerminalMode::Visible);
    termios after{};
    assert(tcgetattr(slave,&after)==0 && after.c_lflag==flags.c_lflag);
    const std::set<dev_t> terminals{device};
    const auto number=std::to_string(static_cast<std::uint32_t>(device));
    auto stat=[&](const std::string &name,long group=123,long foreground=123,char state='S') {
        return "123 ("+name+") "+state+" 1 "+std::to_string(group)+" 123 "+number+" "+std::to_string(foreground)+" 0";
    };
    assert(dt::foregroundAuthentication(stat("sudo"),terminals));
    assert(dt::foregroundAuthentication(stat("sudo-rs"),terminals));
    assert(dt::foregroundAuthentication(stat("su"),terminals));
    assert(dt::foregroundAuthentication(stat("passwd"),terminals));
    assert(!dt::foregroundAuthentication(stat("bash"),terminals));
    assert(!dt::foregroundAuthentication(stat("vim"),terminals));
    assert(!dt::foregroundAuthentication(stat("sudo",123,456),terminals));
    assert(!dt::foregroundAuthentication(stat("sudo",123,123,'T'),terminals));
    assert(!dt::foregroundAuthentication(stat("sudo"),{}));
    assert(!dt::foregroundAuthentication("malformed",terminals));
    flags.c_lflag |= ECHO;
    assert(tcsetattr(slave,TCSANOW,&flags)==0);
    assert(dt::terminalMode(slave,getuid(),device)==dt::TerminalMode::Visible);
    close(slave);close(master);
}
int main() {
    terminalTests();
    dt::Config c; assert(dt::valid(c));
    c.version=0; assert(!dt::valid(c)); c=dt::Config{};
    c.count=0; assert(!dt::valid(c)); c.count=6; assert(!dt::valid(c)); c=dt::Config{};
    c.threshold=99; assert(!dt::valid(c)); c.threshold=1001; assert(!dt::valid(c)); c=dt::Config{};
    c.protect=2; assert(!dt::valid(c)); c=dt::Config{};
    c.keys[1]=c.keys[0]; assert(!dt::valid(c)); c=dt::Config{};
    c.keys[0]=250; assert(!dt::valid(c)); c=dt::Config{};
    assert(dt::modifier(29) && dt::modifier(125) && !dt::modifier(16));
    DoubleTapDetector detector;
    assert(!detector.key_down(16,true,false,1,0,250)); detector.key_up(16);
    assert(detector.key_down(16,true,false,1,200,250)); detector.key_up(16);
    assert(!detector.key_down(16,true,true,1,300,250)); detector.key_up(16);
    assert(!detector.key_down(16,true,false,1,400,250)); detector.key_up(16);
    detector.reset(); assert(!detector.key_down(16,true,false,1,450,250));
    std::puts("PASS: native protocol, detector, PTY modes and foreground authentication checks");
}
