#pragma once

#include <cstdint>
namespace lbds::skip_list::internal {

// A level index, never above MAX_LEVEL - 1, so a byte is the whole range. It
// is stored in every node, where a wider type would cost more per element than
// the value for small T.
using Level = std::uint8_t;

constexpr double P = 0.25;
constexpr Level MAX_LEVEL = 17;

}  // namespace lbds::skip_list::internal
