#include "random_255.h"
#include "constants.h"

#if defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#else
#error "Unsupported platform: no known CSPRNG available"
#endif

#include <cstring>
#include <stdexcept>

namespace bn256 {

void default_random_source(std::span<uint8_t> buf) {
#if defined(__linux__)
   size_t written = 0;
   while (written < buf.size()) {
      ssize_t ret = getrandom(buf.data() + written, buf.size() - written, 0);
      if (ret < 0)
         throw std::runtime_error("getrandom() failed");
      written += static_cast<size_t>(ret);
   }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
   arc4random_buf(buf.data(), buf.size());
#endif
}

// Returns true if a >= constants::order (little-endian limb order)
static bool ge_order(const std::array<uint64_t, 4>& a) {
   for (int i = 3; i >= 0; --i) {
      if (a[i] < constants::order[i]) return false;
      if (a[i] > constants::order[i]) return true;
   }
   return true; // equal
}

std::array<uint64_t, 4> random_scalar(const random_source& rng) {
   std::array<uint64_t, 4> result{};
   for (;;) {
      std::array<uint8_t, 32> buf;
      rng(buf);
      std::memcpy(result.data(), buf.data(), sizeof(result));
      result[3] &= 0x7FFFFFFFFFFFFFFFULL;
      // Reject zero and values >= order
      if (ge_order(result))
         continue;
      if (result[0] == 0 && result[1] == 0 && result[2] == 0 && result[3] == 0)
         continue;
      return result;
   }
}

std::array<uint64_t, 4> random_255() {
   return random_scalar(default_random_source);
}

} // namespace bn256
