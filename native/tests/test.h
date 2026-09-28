#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct TestCase { const char* name; std::function<void()> fn; };
struct TestFailure { std::string msg; };
std::vector<TestCase>& testRegistry();

#define TEST(name) \
    static void name(); \
    static const bool name##_registered = (testRegistry().push_back({#name, name}), true); \
    static void name()

#define CHECK(cond) \
    do { if (!(cond)) throw TestFailure{std::string(__FILE__) + ":" + std::to_string(__LINE__) + ": " #cond}; } while (0)
