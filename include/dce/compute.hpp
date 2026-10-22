#pragma once
#include "common.hpp"
#include <filesystem>
namespace dce {
std::filesystem::path data_path(const std::filesystem::path &root, const std::string &name);
std::vector<json> partition(const json &spec, const std::filesystem::path &root);
json compute(const json &task, const std::filesystem::path &root,
             const std::atomic_bool *cancel = nullptr);
json aggregate(const std::string &type, const std::vector<json> &results);
json serial_reference(const json &spec, const std::filesystem::path &root);
} // namespace dce
