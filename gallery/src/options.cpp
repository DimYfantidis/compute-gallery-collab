#include "options.hpp"

#include <cmath>
#include <format>

OptionValue defaultValue(const DemoOption& option)
{
    return {option.default_number, option.default_flag, option.default_choice};
}

std::string formatValue(const DemoOption& option, const OptionValue& value)
{
    switch (option.type)
    {
    case DemoOption::Type::Int: return std::to_string(std::llround(value.number));
    case DemoOption::Type::Float: return std::format("{:g}", value.number);
    case DemoOption::Type::Bool: return value.flag ? "1" : "0";
    case DemoOption::Type::Choice: return value.choice;
    }
    return {};
}

void appendOptionParameters(const Demo& demo, const std::map<std::string, OptionValue>& values,
                            std::vector<std::string>& args, std::vector<std::pair<std::string, std::string>>& env)
{
    for (const DemoOption& option : demo.options)
    {
        auto it = values.find(option.id);
        const OptionValue value = it != values.end() ? it->second : defaultValue(option);
        const std::string text = formatValue(option, value);

        // A bool's arguments are its on-switch; other types fill in {value}.
        if (option.type != DemoOption::Type::Bool || value.flag)
            for (std::string arg : option.args)
            {
                for (size_t pos; (pos = arg.find("{value}")) != std::string::npos;)
                    arg.replace(pos, 7, text);
                args.push_back(std::move(arg));
            }
        if (!option.env.empty())
            env.emplace_back(option.env, text);
    }
}
