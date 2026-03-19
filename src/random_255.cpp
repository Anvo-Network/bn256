#include "random_255.h"

#if defined(__linux__)
#include <sys/random.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <stdlib.h>
#else
#error "Unsupported platform: no known CSPRNG available"
#endif

#include <stdexcept>

namespace bn256 {

static void secure_random_bytes(void* buf, size_t len) {
#if defined(__linux__)
   size_t written = 0;
   while (written < len) {
      ssize_t ret = getrandom(static_cast<uint8_t*>(buf) + written, len - written, 0);
      if (ret < 0)
         throw std::runtime_error("getrandom() failed");
      written += static_cast<size_t>(ret);
   }
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
   arc4random_buf(buf, len);
#endif
}

std::array<uint64_t, 4> random_255() {
   std::array<uint64_t, 4> result{};
   secure_random_bytes(result.data(), sizeof(result));
   result[3] &= 0x7FFFFFFFFFFFFFFFULL;
   return result;
}

} // namespace bn256
