#include "../include/global.hpp"

namespace fs = std::filesystem;

std::string currentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t_now);

    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string readFile(std::string path) {
    std::string val;
    std::ifstream readingFile(path);
    readingFile >> val;
    return val;
}

