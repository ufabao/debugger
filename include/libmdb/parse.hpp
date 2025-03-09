#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <libmdb/error.hpp>
#include <optional>
#include <string_view>

namespace sdb
{
template <class T>
std::optional<T> to_integral(std::string_view sv, int base = 10)
{
  auto begin = sv.begin();

  // hex numbers can be prefixed with 0x
  if (base == 16 and sv.size() > 1 and begin[0] == '0' and begin[1] == 'x')
  {
    begin += 2;
  }

  T    ret;
  auto result = std::from_chars(begin, sv.end(), ret, base);

  if (result.ptr != sv.end())
  {
    return std::nullopt;
  }

  return ret;
}

template <class F>
std::optional<F> to_float(std::string_view sv)
{
  F    ret;
  auto result = std::from_chars(sv.begin(), sv.end(), ret);

  if (result.ptr != sv.end())
  {
    return std::nullopt;
  }

  return ret;
}

template <std::size_t N>
auto parse_vector(std::string_view text)
{
  auto invalid = [] { sdb::Error::send("Invalid format"); };

  std::array<std::byte, N> bytes;
  const char*              c = text.data();

  if (*c++ != '[')
    invalid();

  for (auto i = 0; i < N - 1; ++i)
  {
    bytes[i] = to_integral<std::byte>({c, 4}, 16).value();
    c += 4;
    if (*c++ != ',')
      invalid();
  }

  bytes[N - 1] = to_integral<std::byte>({c, 4}, 16).value();
  c += 4;

  if (*c++ != ']')
    invalid();
  if (c != text.end())
    invalid();

  return bytes;
}

template <>
inline std::optional<std::byte> to_integral(std::string_view sv, int base)
{
  auto uint8 = to_integral<std::uint8_t>(sv, base);
  if (uint8)
    return static_cast<std::byte>(*uint8);
  return std::nullopt;
}

}  // namespace sdb