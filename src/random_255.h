#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <span>

namespace bn256 {

using random_source = std::function<void(std::span<uint8_t>)>;

void default_random_source(std::span<uint8_t> buf);

// Generate a random scalar uniformly distributed in [1, Order)
std::array<uint64_t, 4> random_scalar(const random_source& rng);

// Legacy internal API — delegates to random_scalar(default_random_source)
std::array<uint64_t, 4> random_255();

} // namespace bn256
