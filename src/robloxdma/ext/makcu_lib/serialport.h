#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <future>
#include <functional>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace makcu {

    struct PendingCommand {
        int command_id;
        std::string command;
        std::promise<std::string> promise;
        std::chrono::steady_clock::time_point timestamp;
        bool expect_response;
        std::chrono::milliseconds timeout;

        PendingCommand(int id, const std::string& cmd, bool expect_resp, std::chrono::milliseconds to)
            : command_id(id), command(cmd), expect_response(expect_resp), timeout(to) {
            timestamp = std::chrono::steady_clock::now();
        }
    };

    class SerialPort {
    public:
        SerialPort();
        ~SerialPort();

        bool open(const std::string& port, uint32_t baudRate);
        void close();
        bool isOpen() const;

        bool setBaudRate(uint32_t baudRate);
        uint32_t getBaudRate() const;
        std::string getPortName() const;

        std::future<std::string> sendTrackedCommand(const std::string& command,
            bool expectResponse = false,
            std::chrono::milliseconds timeout = std::chrono::milliseconds(100));

        bool sendCommand(const std::string& command);

        bool write(const std::vector<uint8_t>& data);
        bool write(const std::string& data);
        std::vector<uint8_t> read(size_t maxBytes = 1024);
        std::string readString(size_t maxBytes = 1024);

        size_t available() const;
        bool flush();

        void setTimeout(uint32_t timeoutMs);
        uint32_t getTimeout() const;

        static std::vector<std::string> getAvailablePorts();
        static std::vector<std::string> findMakcuPorts();

        using ButtonCallback = std::function<void(uint8_t, bool)>;
        void setButtonCallback(ButtonCallback callback);

    private:
        std::string m_portName;
        uint32_t m_baudRate;
        uint32_t m_timeout;
        std::atomic<bool> m_isOpen;
        mutable std::mutex m_mutex;

#ifdef _WIN32
        HANDLE m_handle;
        DCB m_dcb;
        COMMTIMEOUTS m_timeouts;
#else
        int m_fd;
#endif

        std::atomic<int> m_commandCounter{ 0 };
        std::atomic<bool> m_stopListener{ true };

        ButtonCallback m_buttonCallback;

        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;
    };

}
