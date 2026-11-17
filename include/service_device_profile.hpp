#pragma once
// Opt-in metadata experiment for XuperTV Nova on Windows.
// No server tokens, APK identifiers or persistent installation data are changed.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace xs {
struct ServiceDeviceProfile {
    const char* sys_version;
    const char* hardware_info;
    const char* model;
    const char* product;
    const char* display;
    const char* brand;
    const char* ver_id;
    const char* hardware;
    const char* manufacturer;
    const char* device;
    const char* board;
    const char* tags;
    const char* http_package_version;
};

inline bool android_test_profile_requested(const char* value) {
    return value && std::string(value) == "android-test";
}

inline bool nova_android_test_profile_enabled() {
#ifdef _WIN32
    return android_test_profile_requested(std::getenv("NOVA_DEVICE_PROFILE"));
#else
    return false;
#endif
}

inline ServiceDeviceProfile service_device_profile(bool android_test) {
    if (android_test) {
        // Synthetic public metadata for an Android 7.1.1 / OnePlus A5000
        // compatibility experiment, NOT an actual BlueStacks fingerprint.
        return {"7.1.1", "qcom", "ONEPLUS A5000", "ONEPLUS A5000",
                "Android 7.1.1", "OnePlus", "7.1.1", "qcom", "OnePlus",
                "OnePlus5", "msm8998", "release-keys", "Android 7.1.1"};
    }
    // Preserve exactly the original Switch profile in normal mode.
    return {"Horizon", "Tegra X1", "Nintendo Switch", "Horizon homebrew",
            "Horizon", "Nintendo", "Horizon", "Tegra X1", "Nintendo",
            "Nintendo Switch", "nx", "homebrew", "Horizon homebrew"};
}

inline std::string local_test_android_id(const std::string& installation_serial) {
    // Reuse only this installation's existing locally generated serial as a
    // stable synthetic Android ID. Never copy an emulator or third-party ID.
    if (installation_serial.size() < 16) return {};
    const std::string id = installation_serial.substr(0, 16);
    const bool hex = std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isxdigit(c) != 0;
    });
    return hex ? id : std::string{};
}
} // namespace xs
