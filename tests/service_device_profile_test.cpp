#include "service_device_profile.hpp"
#include <iostream>
#include <string>

static bool check(bool condition, const char* label) {
    if (!condition) std::cerr << "Profile test failed: " << label << '\n';
    return condition;
}
int main() {
    bool ok = true;
    ok &= check(!xs::android_test_profile_requested(nullptr), "null disables");
    ok &= check(!xs::android_test_profile_requested(""), "empty disables");
    ok &= check(!xs::android_test_profile_requested("Android-test"), "case sensitive");
    ok &= check(!xs::android_test_profile_requested("android"), "unknown disables");
    ok &= check(xs::android_test_profile_requested("android-test"), "explicit opt in");
    const auto sw = xs::service_device_profile(false);
    ok &= check(std::string(sw.sys_version)=="Horizon", "Switch sysVersion unchanged");
    ok &= check(std::string(sw.hardware_info)=="Tegra X1", "Switch chipset unchanged");
    ok &= check(std::string(sw.model)=="Nintendo Switch", "Switch model unchanged");
    ok &= check(std::string(sw.http_package_version)=="Horizon homebrew", "Switch header unchanged");
    const auto trial = xs::service_device_profile(true);
    ok &= check(std::string(trial.sys_version)=="7.1.1", "Android test sysVersion");
    ok &= check(std::string(trial.model)=="ONEPLUS A5000", "Android test model");
    ok &= check(std::string(trial.brand)=="OnePlus", "Android test brand");
    ok &= check(std::string(trial.http_package_version)=="Android 7.1.1", "Android test header");
    ok &= check(xs::local_test_android_id("0123456789abcdef0123456789abcdef")=="0123456789abcdef", "local ID stable");
    ok &= check(xs::local_test_android_id("invalid-serial").empty(), "reject invalid ID");
    ok &= check(xs::local_test_android_id("short").empty(), "reject short ID");
    return ok ? 0 : 1;
}
