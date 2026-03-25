#include "json_output.h"
#include <cstdio>

namespace json_output {

int success(const nlohmann::json& data)
{
    std::string out = data.dump(2);
    printf("%s\n", out.c_str());
    return 0;
}

int error(const std::string& message)
{
    nlohmann::json j;
    j["status"] = "error";
    j["message"] = message;
    std::string out = j.dump(2);
    printf("%s\n", out.c_str());
    return 1;
}

}  // namespace json_output
