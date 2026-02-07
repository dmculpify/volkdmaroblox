#include "makcu.h"
#include <settings/settings.h>
#include <serialport.h>
#include <cstdio>
#include <cstring>
#include <chrono>

namespace makcu {

    static SerialPort s_port;
    static int s_last_screen_w = 0;
    static int s_last_screen_h = 0;
    static std::chrono::steady_clock::time_point s_last_move_time;
    static constexpr int MIN_MOVE_INTERVAL_MS = 12;

    static bool can_send_move() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_move_time).count();
        if (elapsed >= MIN_MOVE_INTERVAL_MS) {
            s_last_move_time = now;
            return true;
        }
        return false;
    }

    bool connect() {
        if (!settings::aimbot::com_port[0]) return false;
        if (s_port.open(settings::aimbot::com_port, 4000000)) return true;
        return s_port.open(settings::aimbot::com_port, 115200);
    }

    void disconnect() {
        s_port.close();
        s_last_screen_w = s_last_screen_h = 0;
    }

    bool is_connected() {
        return s_port.isOpen();
    }

    void auto_connect() {
        if (s_port.isOpen())
            return;
        if (settings::aimbot::com_port[0]) {
            if (s_port.open(settings::aimbot::com_port, 4000000)) return;
            if (s_port.open(settings::aimbot::com_port, 115200)) return;
        }
        auto ports = SerialPort::findMakcuPorts();
        if (!ports.empty() && (s_port.open(ports[0], 4000000) || s_port.open(ports[0], 115200))) {
            snprintf(settings::aimbot::com_port, sizeof(settings::aimbot::com_port), "%s", ports[0].c_str());
        }
    }

    bool move_relative(int dx, int dy) {
        if (!s_port.isOpen() && !connect())
            return false;
        if (!can_send_move())
            return true;
        char buf[64];
        snprintf(buf, sizeof(buf), "km.move(%d,%d)", dx, dy);
        return s_port.sendCommand(buf);
    }

    bool move_to(int x, int y, int screen_w, int screen_h) {
        if (!s_port.isOpen() && !connect())
            return false;
        if (!can_send_move())
            return true;
        if (screen_w != s_last_screen_w || screen_h != s_last_screen_h) {
            char buf[64];
            snprintf(buf, sizeof(buf), "km.screen(%d,%d)", screen_w, screen_h);
            if (!s_port.sendCommand(buf))
                return false;
            s_last_screen_w = screen_w;
            s_last_screen_h = screen_h;
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "km.moveto(%d,%d)", x, y);
        return s_port.sendCommand(buf);
    }

    bool click(int button) {
        if (!s_port.isOpen() && !connect())
            return false;
        char buf[64];
        snprintf(buf, sizeof(buf), "km.click(%d)", button);
        return s_port.sendCommand(buf);
    }
}
