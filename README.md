# bn256

[![CI](https://github.com/Anvo-Network/bn256/actions/workflows/actions.yml/badge.svg)](https://github.com/Anvo-Network/bn256/actions/workflows/actions.yml)

A C++20 implementation of the Optimal Ate pairing over a 256-bit Barreto-Naehrig curve (BN256). Provides bilinear group operations across three groups (G1, G2, GT) suitable for pairing-based cryptographic protocols.

Based on the [cloudflare/bn256](https://github.com/ethereum/go-ethereum/tree/master/crypto/bn256/cloudflare) Go implementation from go-ethereum.

> **Note:** This curve was previously believed to provide 128-bit security. Recent advances in number field sieve algorithms have reduced the effective security level. See [this discussion](https://moderncrypto.org/mail-archive/curves/2016/000740.html) for details.

## Features

- **G1, G2, GT group operations** -- scalar multiplication, addition, negation
- **Optimal Ate pairing** -- `pair()`, `miller()`, `pairing_check()`
- **Marshaling** -- serialize/deserialize group elements with input validation
- **Secure random generation** -- OS-backed CSPRNG with rejection sampling for uniform scalars in [1, Order)
- **Custom entropy sources** -- caller-injectable `random_source` for testing or hardware RNG integration
- **No external dependencies** -- header-only math, only depends on the C++ standard library and OS random APIs

## Requirements

- C++20 compiler (GCC 10+, Clang 12+, Apple Clang 13+)
- CMake 3.12+
- Linux (x86-64, ARM64) or macOS

## Building

```bash
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild -S.
cmake --build build
```

### Build options

| Option | Default | Description |
|---|---|---|
| `BN256_ENABLE_TEST` | `ON` | Build test suite (requires Catch2 submodule) |
| `BN256_ENABLE_BMI2` | `OFF` | Enable BMI2 instruction set for supported x86-64 targets |

### Running tests

```bash
cd build && ctest
```

## Usage

### Key generation

```cpp
#include <bn256/bn256.h>

// Generate a random private key and corresponding G1 public key
auto [private_key, public_key_g1] = bn256::random_g1();

// Same for G2
auto [secret, public_key_g2] = bn256::random_g2();

// Or generate just a random scalar
auto scalar = bn256::random_scalar();
```

### Custom random source

```cpp
// Provide your own entropy source (e.g., hardware RNG, deterministic for tests)
auto my_rng = [](std::span<uint8_t> buf) {
    // fill buf with random bytes
    my_hardware_rng_fill(buf.data(), buf.size());
};

auto [key, point] = bn256::random_g1(my_rng);
```

### Scalar multiplication

```cpp
bn256::uint255_t scalar = {42, 0, 0, 0};
auto g1_point = bn256::g1::scalar_base_mult(scalar);
auto g2_point = bn256::g2::scalar_base_mult(scalar);
```

### Pairing

```cpp
auto result = bn256::pair(g1_point, g2_point);
```

### Tripartite Diffie-Hellman key agreement

```cpp
// Three parties each generate a private key
auto [a, pa] = bn256::random_g1();
auto [_, qa] = bn256::random_g2();  // recompute with same scalar a

auto [b, pb] = bn256::random_g1();
auto [__, qb] = bn256::random_g2();

auto [c, pc] = bn256::random_g1();
auto [___, qc] = bn256::random_g2();

// Each party computes the shared key
auto k1 = bn256::pair(pb, qc).scalar_mult(a);
auto k2 = bn256::pair(pc, qa).scalar_mult(b);
auto k3 = bn256::pair(pa, qb).scalar_mult(c);
// k1 == k2 == k3
```

### Marshaling

```cpp
// Serialize
auto bytes = g1_point.marshal();  // std::array<uint8_t, 64>

// Deserialize (with validation)
bn256::g1 recovered;
if (auto err = recovered.unmarshal(bytes); err) {
    // handle error: COORDINATE_EXCEEDS_MODULUS, MALFORMED_POINT, etc.
}
```

### Pairing check (batch verification)

```cpp
// Verify e(a[0], b[0]) * e(a[1], b[1]) * ... == 1
std::vector<bn256::g1> a_points = { ... };
std::vector<bn256::g2> b_points = { ... };
bool ok = bn256::pairing_check(a_points, b_points);
```

## API Reference

### Types

| Type | Description |
|---|---|
| `uint255_t` | 255-bit integer as `std::array<uint64_t, 4>` (little-endian) |
| `g1` | Element of G1 (64-byte marshaled) |
| `g2` | Element of G2 (128-byte marshaled) |
| `gt` | Element of GT (384-byte marshaled) |
| `random_source` | `std::function<void(std::span<uint8_t>)>` -- caller-provided RNG |

### Free functions

| Function | Description |
|---|---|
| `pair(g1, g2) -> gt` | Compute Optimal Ate pairing |
| `miller(g1, g2) -> gt` | Apply Miller's algorithm (call `.finalize()` for full pairing) |
| `pairing_check(span<g1>, span<g2>) -> bool` | Batch pairing verification |
| `random_scalar(rng?) -> uint255_t` | Random scalar in [1, Order) |
| `random_g1(rng?) -> tuple<uint255_t, g1>` | Random scalar + G1 generator point |
| `random_g2(rng?) -> tuple<uint255_t, g2>` | Random scalar + G2 generator point |
| `default_random_source(span<uint8_t>)` | OS-backed CSPRNG (getrandom/arc4random) |

### Marshaled-input functions

| Function | Description |
|---|---|
| `g1_add(lhs, rhs, result) -> int32_t` | Add two marshaled G1 points |
| `g1_scalar_mul(point, scalar, result) -> int32_t` | Multiply marshaled G1 point by big-endian scalar |
| `pairing_check(bytes, yield) -> int32_t` | Batch pairing check from marshaled G1/G2 pairs |

## Curve parameters

- **Curve:** BN256 (Barreto-Naehrig)
- **Field prime (p):** 21888242871839275222246405745257275088696311157297823662689037894645226208583
- **Group order:** 21888242871839275222246405745257275088548364400416034343698204186575808495617
- **Embedding degree:** 12
- **Pairing:** Optimal Ate

## Project structure

```
bn256/
  include/bn256/bn256.h   Public API header
  src/                     Implementation
    bn256.cpp              Group operations, pairing, marshaling
    random_255.cpp         CSPRNG and scalar generation
    constants.h            Curve parameters (p, Order, u)
    curve.h                G1 curve point arithmetic
    twist.h                G2 twist point arithmetic
    gfp.h                  Base field (Fp) element
    gfp2.h, gfp6.h, gfp12.h   Extension field towers
    optate.h               Optimal Ate pairing implementation
    lattice.h              Lattice decomposition for scalar multiplication
  test/                    Test suite (Catch2)
  .github/workflows/      CI configuration
```

## CI

Tested on:
- Ubuntu 22.04 (GCC)
- Ubuntu 24.04 (GCC)
- macOS (Apple Clang)

## License

[MIT](./LICENSE)

Copyright (c) 2026 Stratovera LLC and its contributors.

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for guidelines.
