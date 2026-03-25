#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace json_output {

// Print JSON to stdout and return exit code 0
int success(const nlohmann::json& data);

// Print error JSON to stdout and return exit code 1
int error(const std::string& message);

}  // namespace json_output
