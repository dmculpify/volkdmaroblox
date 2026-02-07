#pragma once

#include <cstdint>

namespace makcu {

    // Returns true if connected and move was sent
    bool move_relative(int dx, int dy);

    // Send mouse click to game PC (0=left, 1=right, 2=middle)
    bool click(int button = 0);

    // move to absolute position (game screen space)
    bool move_to(int x, int y, int screen_w, int screen_h);

    // Call once at startup or when com_port changes. Keeps handle open.
    bool connect();
    void disconnect();
    bool is_connected();

    // Tries configured port, then COM3-7. Updates com_port if auto-detected.
    void auto_connect();
}
