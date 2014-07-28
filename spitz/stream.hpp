#ifndef __SPITZ_CPP_STREAM_HPP__
#define __SPITZ_CPP_STREAM_HPP__

#include <spitz/barray.h>

namespace spitz {
  class Stream {
    private:
      struct byte_array ba;

    public:
      Stream(size_t cap) {
        byte_array_init(&ba, cap);
      }

      stream& operator<<(const uint8_t& v)  { _byte_array_pack8(&ba, v); return *this; }
      stream& operator>>(      uint8_t& v)  { _byte_array_unpack8(&ba, &v); return *this; }

      stream& operator<<(const uint16_t& v) { _byte_array_pack16(&ba, v); return *this; }
      stream& operator>>(      uint16_t& v) { _byte_array_unpack16(&ba, &v); return *this; }

      stream& operator<<(const uint32_t& v) { _byte_array_pack32(&ba, v); return *this; }
      stream& operator>>(      uint32_t& v) { _byte_array_unpack32(&ba, &v); return *this; }

      stream& operator<<(const uint64_t& v) { _byte_array_pack64(&ba, v); return *this; }
      stream& operator>>(      uint64_t& v) { _byte_array_unpack64(&ba, &v); return *this; }

      stream& operator<<(const int& v)      { byte_array_pack32(&ba, v); return *this; }
      stream& operator>>(      int& v)      { byte_array_unpack32(&ba, &v); return *this; }

      stream& operator<<(const float& v)    { byte_array_pack32(&ba, v); return *this; }
      stream& operator>>(      float& v)    { byte_array_unpack32(&ba, &v); return *this; }

      stream& operator<<(const double& v)   { byte_array_pack64(&ba, v); return *this; }
      stream& operator>>(      double& v)   { byte_array_unpack64(&ba, &v); return *this; }
  };
};

#endif /* __SPITZ_CPP_STREAM_HPP__ */

// vim:ft=cpp:sw=2:et:sta
