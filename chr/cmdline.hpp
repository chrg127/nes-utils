#pragma once

#include <cstdio>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cmdline {

enum class ParamType { None, Single };

struct Argument {
    char short_opt;
    std::string_view long_opt;
    std::string_view desc;
    ParamType param_type = ParamType::None;
    std::string_view default_param = "";
};

using ArgumentList = std::vector<Argument>;

struct Result {
    std::unordered_map<char, bool> has;
    std::unordered_map<char, std::string_view> params;
    std::vector<std::string_view> items;
};

Result parse(const std::vector<std::string_view> &args, const ArgumentList &valid_args);

inline Result parse(int argc, char **argv, const ArgumentList &valid_args)
{
    return parse(std::vector<std::string_view>{argv, argv + argc}, valid_args);
}

void print_args(const std::vector<Argument> &valid_args, FILE *f = stdout);

} // namespace cmdline
