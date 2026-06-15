#pragma once

#include <string>

class ADBConnect {
public:
    std::string command(const std::string& cmd);

    std::string adb_command(const std::string& device, const std::string& cmd);

    std::string choose_device();

    std::string captureScreen(const std::string& device, const std::string& parentfolder);

    void click(const std::string& device, const int& x, const int& y);
};