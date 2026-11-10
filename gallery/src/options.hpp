#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "catalog.hpp"

// Current value of one demo option; which member is used depends on the option's type.
struct OptionValue
{
    double number = 0.0;
    bool flag = false;
    std::string choice;
};

OptionValue defaultValue(const DemoOption& option);

// The value as the demo receives it: "1000", "0.5", "1"/"0" for a bool, the choice text.
std::string formatValue(const DemoOption& option, const OptionValue& value);

// Appends the arguments and environment variables that pass `values` (by option id; missing
// ones use the default) to the demo, following each option's "args" and "env".
void appendOptionParameters(const Demo& demo, const std::map<std::string, OptionValue>& values,
                            std::vector<std::string>& args, std::vector<std::pair<std::string, std::string>>& env);
