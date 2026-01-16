#ifndef MI_E2EE_SHARD_BUFFER_POOL_H
#define MI_E2EE_SHARD_BUFFER_POOL_H

#include "../common/buffer_pool.h"

namespace mi::shard {

using ByteBufferPool = mi::common::ByteBufferPool;
using ScopedBuffer = mi::common::ScopedBuffer;

inline void SecureWipe(std::uint8_t* data, std::size_t len) {
  mi::common::SecureWipe(data, len);
}

inline ByteBufferPool& GlobalByteBufferPool() {
  return mi::common::GlobalByteBufferPool();
}

}  // namespace mi::shard

#endif  // MI_E2EE_SHARD_BUFFER_POOL_H
