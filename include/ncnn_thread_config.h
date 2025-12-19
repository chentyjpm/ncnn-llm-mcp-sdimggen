#pragma once

#include <cerrno>
#include <climits>
#include <cstdlib>

inline int ncnn_num_threads_from_env()
{
    const char* value = std::getenv("SD_NCNN_NUM_THREADS");
    if (!value || *value == '\0')
        value = std::getenv("NCNN_NUM_THREADS");
    if (!value || *value == '\0')
        return 0;

    errno = 0;
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0')
        return 0;
    if (parsed < 1 || parsed > INT_MAX)
        return 0;
    return static_cast<int>(parsed);
}
