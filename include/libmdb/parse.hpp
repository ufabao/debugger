#pragma once

#include <charconv>
#include <cstdint>
#include <libmdb/error.hpp>
#include <libmdb/registers.hpp>
#include <optional>
#include <string_view>

namespace mdb
{

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

template <class I>
std::optional<I> to_integral(std::string_view sv, int base = 10)
{
  auto begin = sv.begin();
  if (base == 16 and sv.size() > 1 and begin[0] == '0' and begin[1] == 'x')
  {
    begin += 2;
  }

  I    ret;
  auto result = std::from_chars(begin, sv.end(), ret, base);

  if (result.ptr != sv.end())
  {
    return std::nullopt;
  }
  return ret;
}

template <>
std::optional<std::byte> mdb::to_integral(std::string_view sv, int base)
{
  auto uint8 = mdb::to_integral<std::uint8_t>(sv, base);
  if (uint8)
    return static_cast<std::byte>(*uint8);
  return std::nullopt;
}

template <std::size_t N>
auto parse_vector(std::string_view text)
{
  auto invalid = [] { mdb::error::send("Invalid format"); };

  std::array<std::byte, N> bytes;
  const char*              c = text.data();

  if (*c++ != '[')
    invalid();
  for (auto i = 0; i < N - 1; ++i)
  {
    bytes[i] = mdb::to_integral<std::byte>({c, 4}, 16).value();
    c += 4;
    if (*c++ != ',')
      invalid();
  }

  bytes[N - 1] = mdb::to_integral<std::byte>({c, 4}, 16).value();
  c += 4;

  if (*c++ != ']')
    invalid();
  if (c != text.end())
    invalid();

  return bytes;
}

mdb::registers::value parse_register_value(mdb::register_info info, std::string_view text)
{
  try
  {
    if (info.format == mdb::register_format::uint)
    {
      switch (info.size)
      {
        case 1:
          return to_integral<std::uint8_t>(text, 16).value();
        case 2:
          return to_integral<std::uint16_t>(text, 16).value();
        case 4:
          return to_integral<std::uint32_t>(text, 16).value();
        case 8:
          return to_integral<std::uint64_t>(text, 16).value();
      }
    }
    else if (info.format == mdb::register_format::double_float)
    {
      return to_float<double>(text).value();
    }
    else if (info.format == mdb::register_format::long_double)
    {
      return to_float<long double>(text).value();
    }
    else if (info.format == mdb::register_format::vector)
    {
      if (info.size == 8)
      {
        return parse_vector<8>(text);
      }
      else if (info.size == 16)
      {
        return parse_vector<16>(text);
      }
    }
  }
  catch (...)
  {
  }
  mdb::error::send("Invalid format");
}

inline auto parse_vector(std::string_view text)
{
  auto invalid = [] { mdb::error::send("Invalid format"); };

  std::vector<std::byte> bytes;
  const char*            c = text.data();

  if (*c++ != '[')
    invalid();

  while (*c != ']')
  {
    auto byte = mdb::to_integral<std::byte>({c, 4}, 16);
    bytes.push_back(byte.value());
    c += 4;

    if (*c == ',')
      ++c;
    else if (*c != ']')
      invalid();
  }

  if (++c != text.end())
    invalid();

  return bytes;
}

}  // namespace mdb