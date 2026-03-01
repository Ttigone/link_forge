/**
 * @file Version.h
 * @brief LinkForge Framework Version Information
 */

#pragma once

#define LINKFORGE_VERSION_MAJOR 1
#define LINKFORGE_VERSION_MINOR 0
#define LINKFORGE_VERSION_PATCH 0

#define LINKFORGE_VERSION_STRING "1.0.0"
#define LINKFORGE_VERSION ((LINKFORGE_VERSION_MAJOR << 16) | \
                          (LINKFORGE_VERSION_MINOR << 8) | \
                          LINKFORGE_VERSION_PATCH)

namespace LinkForge {

struct Version {
    static constexpr int Major = LINKFORGE_VERSION_MAJOR;
    static constexpr int Minor = LINKFORGE_VERSION_MINOR;
    static constexpr int Patch = LINKFORGE_VERSION_PATCH;
    static constexpr const char* String = LINKFORGE_VERSION_STRING;
    static constexpr const char* BuildDate = __DATE__;
    static constexpr const char* BuildTime = __TIME__;
};

} // namespace LinkForge
