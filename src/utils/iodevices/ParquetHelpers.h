#pragma once

#include <config.h>

#ifdef HAVE_PARQUET
// Helper function to convert various types to Parquet-compatible types
template <typename T>
auto convertToParquetType(const T& value) {
    if constexpr (std::is_same_v<T, unsigned long>) {
        if constexpr (sizeof(unsigned long) <= sizeof(uint32_t)) {
            return static_cast<uint32_t>(value);
        } else {
            return static_cast<uint64_t>(value);
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        return value;
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            if constexpr (sizeof(T) <= 1) return static_cast<int8_t>(value);
            else if constexpr (sizeof(T) <= 2) return static_cast<int16_t>(value);
            else if constexpr (sizeof(T) <= 4) return static_cast<int32_t>(value);
            else return static_cast<int64_t>(value);
        } else {
            if constexpr (sizeof(T) <= 1) return static_cast<uint8_t>(value);
            else if constexpr (sizeof(T) <= 2) return static_cast<uint16_t>(value);
            else if constexpr (sizeof(T) <= 4) return static_cast<uint32_t>(value);
            else return static_cast<uint64_t>(value);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) <= 4) return static_cast<float>(value);
        else return static_cast<double>(value);
    } else if constexpr (std::is_same_v<T, std::chrono::milliseconds> || 
                         std::is_same_v<T, std::chrono::microseconds>) {
        return value;
    } else if constexpr (std::is_same_v<T, char>) {
        return value;
    } else if constexpr (std::is_array_v<T>) {
        // try the toString function
        return  toString(value);
    } else if constexpr (std::is_same_v<T, const char*> || 
                         std::is_same_v<T, std::string> || 
                         std::is_same_v<T, std::string_view>) {
        // have to take a copy of the string, to ensure its lifetime is long enough
        return std::string(value);
    } else {
        // For any other type, convert to string
        return toString(value);
    }
}
#endif