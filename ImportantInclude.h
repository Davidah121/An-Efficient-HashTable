#pragma once
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

#include "rapidhash.h"

#ifndef LIKELY
#define LIKELY(x)      __builtin_expect(!!(x), 1)
#define UNLIKELY(x)    __builtin_expect(!!(x), 0)
#endif

//base case for all objects
template<typename T>
constexpr inline uint64_t testHash(const T& key)
{
    return rapidhash(&key, sizeof(T));
}

//process each character in the strings for any string type that extends std::basic_string (std::string, std::wstring)
template<typename T>
constexpr inline uint64_t testHash(const std::basic_string<T>& key)
{
    return rapidhash(key.data(), key.size()*sizeof(T));
}
template<typename T>
constexpr inline uint64_t testHash(const std::basic_string_view<T>& key)
{
    return rapidhash(key.data(), key.size()*sizeof(T));
}

//base case for all numbers. Not the identity
constexpr inline uint64_t testHash(const uint64_t& key)
{
    return rapid_mix(key, UINT64_C(0x9E3779B97F4A7C15));
}

//for all integer types, you can just zero extend up to 64bits
template<typename T>
std::enable_if<std::is_integral_v<T>, uint64_t>
constexpr inline testHash(const T& key)
{
    return testHash((uint64_t)key);
}

//for floats and doubles, you want to preserve all the fractional bits too so casting directly is not ideal
//type pun in order to preserve all the bits
constexpr inline uint64_t testHash(const float& key)
{
    uint64_t k = *((uint32_t*)&key);
    return testHash(k);
}
constexpr inline uint64_t testHash(const double& key)
{
    uint64_t k = *((uint64_t*)&key);
    return testHash(k);
}

template<typename K>
struct TestHashFunction
{
    std::size_t operator()(const K& k) const noexcept
    {
        return testHash(k);
    }
};