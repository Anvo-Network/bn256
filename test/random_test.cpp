#include <bn256/bn256.h>
#include <constants.h>
#include <random_255.h>
#include <catch2/catch_test_macros.hpp>
#include <cstring>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif

// Helper: returns true if a < b (little-endian limb order)
static bool lt_order(const bn256::uint255_t& a) {
   for (int i = 3; i >= 0; --i) {
      if (a[i] < bn256::constants::order[i]) return true;
      if (a[i] > bn256::constants::order[i]) return false;
   }
   return false; // equal
}

static bool is_zero(const bn256::uint255_t& a) {
   return a[0] == 0 && a[1] == 0 && a[2] == 0 && a[3] == 0;
}

// ─── random_scalar tests ────────────────────────────────────────────────

TEST_CASE("random_scalar produces values in [1, Order)", "[random]") {
   for (int i = 0; i < 100; ++i) {
      auto k = bn256::random_scalar();
      REQUIRE(!is_zero(k));
      REQUIRE(lt_order(k));
   }
}

TEST_CASE("random_scalar accepts a custom random source", "[random]") {
   // Deterministic source that fills with a known pattern
   int call_count = 0;
   auto deterministic_rng = [&](std::span<uint8_t> buf) {
      // Fill with a small known value that is < Order
      std::memset(buf.data(), 0, buf.size());
      buf[0] = static_cast<uint8_t>(42 + call_count);
      ++call_count;
   };

   auto k = bn256::random_scalar(deterministic_rng);
   CHECK(!is_zero(k));
   CHECK(lt_order(k));
   CHECK(call_count >= 1);
}

TEST_CASE("random_scalar rejects values >= Order", "[random]") {
   // Source that first returns Order itself, then a valid value
   int call = 0;
   auto rng = [&](std::span<uint8_t> buf) {
      bn256::uint255_t val;
      if (call == 0) {
         // Return exactly Order (should be rejected)
         val = bn256::constants::order;
      } else if (call == 1) {
         // Return Order + 1 (should be rejected)
         val = bn256::constants::order;
         val[0] += 1;
      } else {
         // Return 1 (valid)
         val = {1, 0, 0, 0};
      }
      std::memcpy(buf.data(), val.data(), 32);
      ++call;
   };

   auto k = bn256::random_scalar(rng);
   CHECK(call == 3); // First two rejected, third accepted
   CHECK(k == bn256::uint255_t{1, 0, 0, 0});
}

TEST_CASE("random_scalar rejects zero", "[random]") {
   int call = 0;
   auto rng = [&](std::span<uint8_t> buf) {
      if (call == 0) {
         // Return all zeros (should be rejected)
         std::memset(buf.data(), 0, buf.size());
      } else {
         // Return 7 (valid)
         std::memset(buf.data(), 0, buf.size());
         buf[0] = 7;
      }
      ++call;
   };

   auto k = bn256::random_scalar(rng);
   CHECK(call == 2);
   CHECK(k[0] == 7);
}

// ─── random_g1 / random_g2 tests ────────────────────────────────────────

TEST_CASE("random_g1 produces valid curve points", "[random]") {
   for (int i = 0; i < 10; ++i) {
      auto [k, point] = bn256::random_g1();
      REQUIRE(!is_zero(k));
      REQUIRE(lt_order(k));

      // Verify the point marshals and unmarshals (implies it is on curve)
      auto marshaled = point.marshal();
      bn256::g1 recovered;
      REQUIRE(recovered.unmarshal(marshaled) == std::error_code{});
      // Compare via marshaled bytes since internal projective coords may differ
      CHECK(recovered.marshal() == marshaled);
   }
}

TEST_CASE("random_g2 produces valid curve points", "[random]") {
   for (int i = 0; i < 10; ++i) {
      auto [k, point] = bn256::random_g2();
      REQUIRE(!is_zero(k));
      REQUIRE(lt_order(k));

      auto marshaled = point.marshal();
      bn256::g2 recovered;
      REQUIRE(recovered.unmarshal(marshaled) == std::error_code{});
      CHECK(recovered.marshal() == marshaled);
   }
}

TEST_CASE("random_g1 with custom source matches scalar_base_mult", "[random]") {
   auto [k, point] = bn256::random_g1();
   auto expected = bn256::g1::scalar_base_mult(k);
   CHECK(point == expected);
}

TEST_CASE("random_g2 with custom source matches scalar_base_mult", "[random]") {
   auto [k, point] = bn256::random_g2();
   auto expected = bn256::g2::scalar_base_mult(k);
   CHECK(point == expected);
}

// ─── Non-determinism test ───────────────────────────────────────────────

TEST_CASE("random_scalar produces distinct values", "[random]") {
   auto a = bn256::random_scalar();
   auto b = bn256::random_scalar();
   // Probability of collision is ~2^-254, so this should always pass
   CHECK(a != b);
}

// ─── Adversarial unmarshal tests ────────────────────────────────────────

TEST_CASE("g1 unmarshal rejects coordinate equal to field modulus p", "[adversarial]") {
   // x = p (the field modulus), y = 0
   std::array<uint8_t, 64> data{};
   // p in big-endian: 0x30644E72E131A029 B85045B68181585D 97816A916871CA8D 3C208C16D87CFD47
   auto write_be64 = [](uint8_t* dst, uint64_t v) {
      for (int i = 7; i >= 0; --i) { dst[i] = static_cast<uint8_t>(v); v >>= 8; }
   };
   write_be64(data.data() + 0,  0x30644E72E131A029ULL);
   write_be64(data.data() + 8,  0xB85045B68181585DULL);
   write_be64(data.data() + 16, 0x97816A916871CA8DULL);
   write_be64(data.data() + 24, 0x3C208C16D87CFD47ULL);

   bn256::g1 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g1 unmarshal rejects coordinate exceeding field modulus p", "[adversarial]") {
   // x = p + 1
   std::array<uint8_t, 64> data{};
   auto write_be64 = [](uint8_t* dst, uint64_t v) {
      for (int i = 7; i >= 0; --i) { dst[i] = static_cast<uint8_t>(v); v >>= 8; }
   };
   write_be64(data.data() + 0,  0x30644E72E131A029ULL);
   write_be64(data.data() + 8,  0xB85045B68181585DULL);
   write_be64(data.data() + 16, 0x97816A916871CA8DULL);
   write_be64(data.data() + 24, 0x3C208C16D87CFD48ULL); // p + 1

   bn256::g1 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g1 unmarshal rejects all-0xFF coordinates", "[adversarial]") {
   std::array<uint8_t, 64> data;
   std::memset(data.data(), 0xFF, data.size());

   bn256::g1 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g2 unmarshal rejects coordinate equal to field modulus p", "[adversarial]") {
   std::array<uint8_t, 128> data{};
   auto write_be64 = [](uint8_t* dst, uint64_t v) {
      for (int i = 7; i >= 0; --i) { dst[i] = static_cast<uint8_t>(v); v >>= 8; }
   };
   // First 32 bytes = p
   write_be64(data.data() + 0,  0x30644E72E131A029ULL);
   write_be64(data.data() + 8,  0xB85045B68181585DULL);
   write_be64(data.data() + 16, 0x97816A916871CA8DULL);
   write_be64(data.data() + 24, 0x3C208C16D87CFD47ULL);

   bn256::g2 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g1 unmarshal rejects point not on curve", "[adversarial]") {
   // Valid-range but random coordinates almost certainly not on curve
   std::array<uint8_t, 64> data{};
   data[31] = 1; // x = 1
   data[63] = 1; // y = 1

   bn256::g1 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g2 unmarshal rejects point not on curve", "[adversarial]") {
   std::array<uint8_t, 128> data{};
   data[31] = 1;
   data[63] = 1;
   data[95] = 1;
   data[127] = 1;

   bn256::g2 point;
   CHECK(point.unmarshal(data) != std::error_code{});
}

TEST_CASE("g1 unmarshal accepts point at infinity", "[adversarial]") {
   std::array<uint8_t, 64> data{};
   bn256::g1 point;
   CHECK(point.unmarshal(data) == std::error_code{});
}

TEST_CASE("g2 unmarshal accepts point at infinity", "[adversarial]") {
   std::array<uint8_t, 128> data{};
   bn256::g2 point;
   CHECK(point.unmarshal(data) == std::error_code{});
}

TEST_CASE("pairing_check rejects odd-length input", "[adversarial]") {
   auto yield = []() {};
   std::vector<uint8_t> bad(100, 0); // not a multiple of 192
   CHECK(bn256::pairing_check(bad, yield) == -1);
}

TEST_CASE("pairing_check rejects invalid G1 point in pair", "[adversarial]") {
   auto yield = []() {};
   // 192 bytes: first 64 = invalid G1, next 128 = zeros
   std::vector<uint8_t> data(192, 0);
   data[31] = 1; // x = 1
   data[63] = 1; // y = 1 (not on curve)

   CHECK(bn256::pairing_check(data, yield) == -1);
}
