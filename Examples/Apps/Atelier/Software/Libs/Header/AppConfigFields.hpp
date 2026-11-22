#ifndef APP_CONFIG_FIELDS_HPP
#define APP_CONFIG_FIELDS_HPP

#include "SDK/AppConfig/AppConfig.hpp"

#include <cstddef>

namespace AtelierConfig {

constexpr const char *kFileName = "app_config.json";

/// Mirrors app-manifest.json's "configFields"; CI compares the two.
extern const SDK::AppConfig::Field kFields[];
extern const size_t kFieldCount;

} // namespace AtelierConfig

#endif // APP_CONFIG_FIELDS_HPP
