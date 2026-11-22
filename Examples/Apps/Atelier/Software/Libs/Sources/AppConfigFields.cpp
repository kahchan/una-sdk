#include "AppConfigFields.hpp"

namespace AtelierConfig {

using SDK::AppConfig;

// Upper bound = faces built so far; raise it with the table.
const AppConfig::Field kFields[] = {
    AppConfig::intField("face", 0, 0, 8),
};

const size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

} // namespace AtelierConfig
