#ifndef mdb_TYPES_HPP
#define mdb_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mdb
{
using byte64  = std::array<std::byte, 8>;
using byte128 = std::array<std::byte, 16>;

template <class T>
class span
{
 public:
  span() = default;
  span(T* data, std::size_t size) : data_(data), size_(size) {}
  span(T* data, T* end) : data_(data), size_(end - data) {}

  template <class U>
  span(const std::vector<U>& vec) : data_(vec.data()), size_(vec.size())
  {
  }

  T* begin() const
  {
    return data_;
  }
  T* end() const
  {
    return data_ + size_;
  }

  std::size_t size() const
  {
    return size_;
  }
  T& operator[](std::size_t index) const
  {
    return data_[index];
  }

 private:
  T*          data_ = nullptr;
  std::size_t size_ = 0;
};

class virt_addr
{
 public:
  virt_addr() = default;
  explicit virt_addr(std::uint64_t addr) : addr_(addr) {}

  operator std::uint64_t() const
  {
    return addr_;
  }

  std::uint64_t addr() const
  {
    return addr_;
  }

  virt_addr operator+(std::int64_t offset) const
  {
    return virt_addr(addr_ + offset);
  }
  virt_addr operator-(std::int64_t offset) const
  {
    return virt_addr(addr_ - offset);
  }
  virt_addr& operator+=(std::int64_t offset)
  {
    addr_ += offset;
    return *this;
  }
  virt_addr& operator-=(std::int64_t offset)
  {
    addr_ -= offset;
    return *this;
  }
  bool operator==(const virt_addr& other) const
  {
    return addr_ == other.addr_;
  }
  bool operator!=(const virt_addr& other) const
  {
    return addr_ != other.addr_;
  }
  bool operator<(const virt_addr& other) const
  {
    return addr_ < other.addr_;
  }
  bool operator<=(const virt_addr& other) const
  {
    return addr_ <= other.addr_;
  }
  bool operator>(const virt_addr& other) const
  {
    return addr_ > other.addr_;
  }
  bool operator>=(const virt_addr& other) const
  {
    return addr_ >= other.addr_;
  }

 private:
  std::uint64_t addr_ = 0;
};
}  // namespace mdb

#endif