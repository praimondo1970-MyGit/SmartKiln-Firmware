#pragma once

#include <Arduino.h>
#include <cstring>

class SilentSerialWrapper {
public:
    explicit SilentSerialWrapper(HardwareSerial& real) : realSerial(real) {}

    void setLogsEnabled(bool enabled) { logsEnabled = enabled; }
    bool getLogsEnabled() const { return logsEnabled; }

    void begin(unsigned long baud) { realSerial.begin(baud); }
    void begin(unsigned long baud, uint32_t config) { realSerial.begin(baud, config); }
    void updateBaudRate(unsigned long baud) { realSerial.updateBaudRate(baud); }
    void end() { realSerial.end(); }
    void setTimeout(unsigned long timeout) { realSerial.setTimeout(timeout); }

    int available() { return realSerial.available(); }
    int availableForWrite() { return realSerial.availableForWrite(); }
    int read() { return realSerial.read(); }
    int peek() { return realSerial.peek(); }
    String readStringUntil(char terminator) { return realSerial.readStringUntil(terminator); }
    size_t readBytes(char* buffer, size_t length) { return realSerial.readBytes(buffer, length); }

    operator bool() { return static_cast<bool>(realSerial); }

    void println() { if (logsEnabled) realSerial.println(); }

    template <typename T>
    void print(const T& value) { if (logsEnabled) realSerial.print(value); }

    template <typename T>
    void println(const T& value) { if (logsEnabled) realSerial.println(value); }

    void print(const char* value) { if (logsEnabled) realSerial.print(value); }
    void println(const char* value) { if (logsEnabled) realSerial.println(value); }
    void print(char* value) { if (logsEnabled) realSerial.print(value); }
    void println(char* value) { if (logsEnabled) realSerial.println(value); }
    void print(char value) { if (logsEnabled) realSerial.print(value); }
    void println(char value) { if (logsEnabled) realSerial.println(value); }
    void print(unsigned char value) { if (logsEnabled) realSerial.print(value); }
    void println(unsigned char value) { if (logsEnabled) realSerial.println(value); }
    void print(const __FlashStringHelper* value) { if (logsEnabled) realSerial.print(value); }
    void println(const __FlashStringHelper* value) { if (logsEnabled) realSerial.println(value); }

    template <typename... Args>
    size_t printf(const char* format, Args... args) {
        if (!logsEnabled) return 0;
        return realSerial.printf(format, args...);
    }

    size_t write(uint8_t b) { return logsEnabled ? realSerial.write(b) : 1; }
    size_t write(const uint8_t* buffer, size_t size) {
        if (!logsEnabled) return size;
        return realSerial.write(buffer, size);
    }
    size_t write(const char* buffer) {
        if (!logsEnabled) return buffer ? strlen(buffer) : 0;
        return realSerial.write(buffer);
    }

    void flush() { if (logsEnabled) realSerial.flush(); }

private:
    HardwareSerial& realSerial;
    bool logsEnabled = false;
};

extern SilentSerialWrapper SilentSerial;

class SerialLogScope {
public:
    explicit SerialLogScope(SilentSerialWrapper& wrapper, bool enable = true)
        : serial(wrapper), previous(wrapper.getLogsEnabled()) {
        if (enable) {
            serial.setLogsEnabled(true);
        }
    }

    ~SerialLogScope() {
        serial.setLogsEnabled(previous);
    }

private:
    SilentSerialWrapper& serial;
    bool previous;
};

#ifdef Serial
#undef Serial
#endif
#define Serial SilentSerial











