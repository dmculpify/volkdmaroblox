/**
 * SerialPort implementation for MAKCU lib - serial connection and device detection
 */
#include "serialport.h"
#include <Windows.h>
#include <cstdio>

#ifdef _WIN32

namespace makcu {

    SerialPort::SerialPort() : m_baudRate(115200), m_timeout(100), m_isOpen(false),
        m_handle(INVALID_HANDLE_VALUE), m_stopListener(true) {}

    SerialPort::~SerialPort() {
        close();
    }

    bool SerialPort::open(const std::string& port, uint32_t baudRate) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_isOpen.load()) close();
        m_portName = port;
        m_baudRate = baudRate;
        std::string path = "\\\\.\\" + port;
        m_handle = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DCB dcb = {};
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(m_handle, &dcb)) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return false;
        }
        dcb.BaudRate = (baudRate >= 4000000) ? 4000000 : CBR_115200;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        if (!SetCommState(m_handle, &dcb)) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return false;
        }
        COMMTIMEOUTS to = {};
        to.ReadIntervalTimeout = 2;
        to.ReadTotalTimeoutConstant = 10;
        to.WriteTotalTimeoutConstant = 50;
        if (!SetCommTimeouts(m_handle, &to)) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return false;
        }
        m_isOpen.store(true);
        return true;
    }

    void SerialPort::close() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
        m_isOpen.store(false);
    }

    bool SerialPort::isOpen() const {
        return m_isOpen.load();
    }

    bool SerialPort::sendCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        std::string buf = command;
        if (buf.empty() || (buf.back() != '\n' && (buf.size() < 2 || buf[buf.size()-2] != '\r')))
            buf += "\r\n";
        DWORD written = 0;
        if (!WriteFile(m_handle, buf.c_str(), (DWORD)buf.size(), &written, nullptr) || written != buf.size())
            return false;
        FlushFileBuffers(m_handle);
        return true;
    }

    std::vector<std::string> SerialPort::getAvailablePorts() {
        std::vector<std::string> ports;
        for (int i = 1; i <= 20; ++i) {
            char name[16];
            snprintf(name, sizeof(name), "COM%d", i);
            std::string path = "\\\\.\\";
            path += name;
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                CloseHandle(h);
                ports.push_back(name);
            }
        }
        return ports;
    }

    std::vector<std::string> SerialPort::findMakcuPorts() {
        std::vector<std::string> result;
        auto all = getAvailablePorts();
        for (const auto& port : all) {
            SerialPort sp;
            if (sp.open(port, 4000000)) {
                if (sp.sendCommand("km.move(0,0)")) {
                    result.push_back(port);
                    sp.close();
                    return result;
                }
                sp.close();
            }
        }
        for (const auto& port : all) {
            SerialPort sp;
            if (sp.open(port, 115200)) {
                if (sp.sendCommand("km.move(0,0)")) {
                    result.push_back(port);
                    sp.close();
                    return result;
                }
                sp.close();
            }
        }
        return result;
    }

    bool SerialPort::setBaudRate(uint32_t r) { m_baudRate = r; return true; }
    uint32_t SerialPort::getBaudRate() const { return m_baudRate; }
    std::string SerialPort::getPortName() const { return m_portName; }
    void SerialPort::setTimeout(uint32_t ms) { m_timeout = ms; }
    uint32_t SerialPort::getTimeout() const { return m_timeout; }
    size_t SerialPort::available() const { return 0; }
    bool SerialPort::flush() { return true; }
    bool SerialPort::write(const std::vector<uint8_t>&) { return false; }
    bool SerialPort::write(const std::string& s) { return sendCommand(s); }
    std::vector<uint8_t> SerialPort::read(size_t) { return {}; }
    std::string SerialPort::readString(size_t) { return ""; }
    std::future<std::string> SerialPort::sendTrackedCommand(const std::string&, bool, std::chrono::milliseconds) {
        std::promise<std::string> p;
        p.set_value("");
        return p.get_future();
    }
    void SerialPort::setButtonCallback(ButtonCallback) {}
}

#endif
