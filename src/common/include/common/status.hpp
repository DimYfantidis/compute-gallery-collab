#pragma once

#include <concepts>
#include <string>
#include <type_traits>

template <typename RetVal, typename RetCode = bool>
    requires(std::is_enum_v<RetCode> || std::same_as<RetCode, bool>)
struct Status
{
    RetCode code;
    RetVal value;
    std::string message;
};

// Boolean specialization
template <typename RetVal>
struct Status<RetVal, bool>
{
    bool ok;
    RetVal value;
    std::string message;
};
