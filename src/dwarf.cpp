#include <algorithm>
#include <libmdb/bit.hpp>
#include <libmdb/dwarf.hpp>
#include <libmdb/elf.hpp>
#include <libmdb/error.hpp>
#include <libmdb/types.hpp>
#include <string_view>

namespace
{

class cursor
{
 public:
  explicit cursor(mdb::span<const std::byte> data) : data_(data), pos_(data.begin()) {}

  void skip_form(std::uint64_t form)
  {
    switch (form)
    {
      case DW_FORM_flag_present:
        break;
      case DW_FORM_ref1:
      case DW_FORM_flag:
        pos_ += 1;
        break;
      case DW_FORM_data2:
      case DW_FORM_ref2:
        pos_ += 2;
        break;
      case DW_FORM_data4:
      case DW_FORM_ref4:
      case DW_FORM_ref_addr:
      case DW_FORM_sec_offset:
      case DW_FORM_strp:
        pos_ += 4;
        break;
      case DW_FORM_data8:
      case DW_FORM_addr:
        pos_ += 8;
        break;
      case DW_FORM_sdata:
        sleb128();
        break;
      case DW_FORM_udata:
      case DW_FORM_ref_udata:
        uleb128();
        break;
      case DW_FORM_block1:
        pos_ += u8();
        break;
      case DW_FORM_block2:
        pos_ += u16();
        break;
      case DW_FORM_block4:
        pos_ += u32();
        break;
      case DW_FORM_block:
      case DW_FORM_exprloc:
        pos_ += uleb128();
        break;
      case DW_FORM_string:
        while (!finished() && *pos_ != std::byte(0))
        {
          ++pos_;
        }
        ++pos_;
        break;
      case DW_FORM_indirect:
        skip_form(uleb128());
        break;
      default:
        mdb::error::send("Unrecognized DWARF form");
    }
  }

  cursor& operator++()
  {
    ++pos_;
    return *this;
  }

  cursor& operator+=(std::size_t size)
  {
    pos_ += size;
    return *this;
  }

  const std::byte* position() const
  {
    return pos_;
  }

  bool finished() const
  {
    return pos_ >= data_.end();
  }

  template <typename T>
  T fixed_int()
  {
    auto t = mdb::from_bytes<T>(pos_);
    pos_ += sizeof(T);
    return t;
  }

  std::string_view string()
  {
    const auto*      null_terminator = std::find(pos_, data_.end(), std::byte(0));
    std::string_view ret(reinterpret_cast<const char*>(pos_), null_terminator - pos_);
    pos_ = null_terminator + 1;
    return ret;
  }

  std::uint64_t uleb128()
  {
    std::uint64_t res   = 0;
    int           shift = 0;
    std::uint8_t  byte  = 0;

    do
    {
      byte        = u8();
      auto masked = static_cast<uint64_t>(byte & 0x7f);
      res |= masked << shift;
      shift += 7;
    } while ((byte & 0x80) != 0);
    return res;
  }

  std::int64_t sleb128()
  {
    std::uint64_t res   = 0;
    int           shift = 0;
    std::uint8_t  byte  = 0;

    do
    {
      byte        = u8();
      auto masked = static_cast<uint64_t>(byte & 0x7f);
      res |= masked << shift;
      shift += 7;
    } while ((byte & 0x80) != 0);

    if ((shift < sizeof(res) * 8) and (byte & 0x40))
    {
      res |= (~static_cast<std::uint64_t>(0) << shift);
    }

    return res;
  }

  std::uint8_t u8()
  {
    return fixed_int<std::uint8_t>();
  }
  std::uint8_t u16()
  {
    return fixed_int<std::uint16_t>();
  }
  std::uint8_t u32()
  {
    return fixed_int<std::uint32_t>();
  }
  std::uint8_t u64()
  {
    return fixed_int<std::uint64_t>();
  }

  std::uint8_t s8()
  {
    return fixed_int<std::int8_t>();
  }
  std::uint8_t s16()
  {
    return fixed_int<std::int16_t>();
  }
  std::uint8_t s32()
  {
    return fixed_int<std::int32_t>();
  }
  std::uint8_t s64()
  {
    return fixed_int<std::int64_t>();
  }

 private:
  mdb::span<const std::byte> data_;
  const std::byte*           pos_;
};

std::unordered_map<std::uint64_t, mdb::abbrev> parse_abbrev_table(const mdb::elf& obj,
                                                                  std::size_t     offset)
{
  cursor cur(obj.get_section_contents(".debug_abbrev"));
  cur += offset;

  std::unordered_map<std::uint64_t, mdb::abbrev> table;
  std::uint64_t                                  code = 0;
  do
  {
    code              = cur.uleb128();
    auto tag          = cur.uleb128();
    auto has_children = static_cast<bool>(cur.u8());

    std::vector<mdb::attr_spec> attr_specs;
    std::uint64_t               attr = 0;
    do
    {
      attr      = cur.uleb128();
      auto form = cur.uleb128();
      if (attr != 0)
      {
        attr_specs.push_back(mdb::attr_spec{.attr = attr, .form = form});
      }
    } while (attr != 0);

    if (code != 0)
    {
      table.emplace(code,
                    mdb::abbrev{.code         = code,
                                .tag          = tag,
                                .has_children = has_children,
                                .attr_specs   = std::move(attr_specs)});
    }
  } while (code != 0);

  return table;
}

std::unique_ptr<mdb::compile_unit> parse_compile_unit(mdb::dwarf&     dwarf,
                                                      const mdb::elf& obj,
                                                      cursor          cur)
{
  auto start        = cur.position();
  auto size         = cur.u32();
  auto version      = cur.u16();
  auto abbrev       = cur.u32();
  auto address_size = cur.u8();

  if (size == 0xffffffff)
  {
    mdb::error::send("Only DWARF32 is supported");
  }
  if (version != 4)
  {
    mdb::error::send("Only DWARF version 4 is supported");
  }
  if (address_size != 8)
  {
    mdb::error::send("Invalid address size for DWARF");
  }

  size += sizeof(std::uint32_t);
  mdb::span<const std::byte> data = {start, size};
  return std::make_unique<mdb::compile_unit>(dwarf, data, abbrev);
}

std::vector<std::unique_ptr<mdb::compile_unit>> parse_compile_units(mdb::dwarf&     dwarf,
                                                                    const mdb::elf& obj)
{
  auto   debug_info = obj.get_section_contents(".debug_info");
  cursor cur(debug_info);

  std::vector<std::unique_ptr<mdb::compile_unit>> units;
  while (!cur.finished())
  {
    auto unit = parse_compile_unit(dwarf, obj, cur);
    cur += unit->data().size();
    units.push_back(std::move(unit));
  }

  return units;
}

mdb::die parse_die(const mdb::compile_unit& cu, cursor cur)
{
  auto pos         = cur.position();
  auto abbrev_code = cur.uleb128();

  if (abbrev_code == 0)
  {
    const auto* next = cur.position();
    return mdb::die{next};
  }

  auto& abbrev_table = cu.abbrev_table();
  auto& abbrev       = abbrev_table.at(abbrev_code);

  std::vector<const std::byte*> attr_locs;
  attr_locs.reserve(abbrev.attr_specs.size());
  for (auto& attr : abbrev.attr_specs)
  {
    attr_locs.push_back(cur.position());
    cur.skip_form(attr.form);
  }

  auto next = cur.position();
  return {pos, &cu, &abbrev, std::move(attr_locs), next};
}
}  // namespace

const std::unordered_map<std::uint64_t, mdb::abbrev>& mdb::dwarf::get_abbrev_table(
    std::size_t offset)
{
  if (!abbrev_tables_.count(offset))
  {
    abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));
  }
  return abbrev_tables_.at(offset);
}

const std::unordered_map<std::uint64_t, mdb::abbrev>& mdb::compile_unit::abbrev_table() const
{
  return parent_->get_abbrev_table(abbrev_offset_);
}

mdb::dwarf::dwarf(const mdb::elf& parent) : elf_(&parent)
{
  compile_units_ = parse_compile_units(*this, parent);
}

mdb::die mdb::compile_unit::root() const
{
  std::size_t header_size = 11;
  cursor      cur({data_.begin() + header_size, data_.end()});
  return parse_die(*this, cur);
}

mdb::die::children_range::iterator::iterator(const mdb::die& d)
{
  cursor next_cur({d.next_, d.cu_->data().end()});
  die_ = parse_die(*d.cu_, next_cur);
}

bool mdb::die::children_range::iterator::operator==(const iterator& rhs) const
{
  auto lhs_null = !die_.has_value() or !die_->abbrev_entry();
  auto rhs_null = !rhs.die_.has_value() or !rhs.die_->abbrev_entry();

  if (lhs_null and rhs_null)
    return true;
  if (lhs_null or rhs_null)
    return false;

  return die_->abbrev_ == rhs->abbrev_ and die_->next() == rhs->next();
}

mdb::die::children_range::iterator& mdb::die::children_range::iterator::operator++()
{
  if (!die_.has_value() or !die_->abbrev_)
    return *this;

  if (!die_->abbrev_->has_children)
  {
    cursor next_cur({die_->next_, die_->cu_->data().end()});
    die_ = parse_die(*die_->cu_, next_cur);
  }
  else if (die_->contains(DW_AT_sibling))
  {
    die_ = die_.value()[DW_AT_sibling].as_reference();
  }
  else
  {
    iterator sub_children(*die_);
    while (sub_children->abbrev_)
      ++sub_children;
    cursor next_cur({sub_children->next_, die_->cu_->data().end()});
    die_ = parse_die(*die_->cu_, next_cur);
  }
  return *this;
}

mdb::die::children_range::iterator mdb::die::children_range::iterator::operator++(int)
{
  auto tmp = *this;
  ++(*this);
  return tmp;
}

mdb::die::children_range mdb::die::children() const
{
  return {*this};
}

bool mdb::die::contains(std::uint64_t attribute) const
{
  auto& specs = abbrev_->attr_specs;
  return std::find_if(begin(specs),
                      end(specs),
                      [=](auto spec) { return spec.attr == attribute; }) != end(specs);
}

mdb::attr mdb::die::operator[](std::uint64_t attribute) const
{
  auto& specs = abbrev_->attr_specs;
  for (std::size_t i = 0; i < specs.size(); ++i)
  {
    if (specs[i].attr == attribute)
    {
      return {cu_, specs[i].attr, specs[i].form, attr_locs_[i]};
    }
  }

  error::send("Attribute not found");
}

mdb::file_addr mdb::attr::as_address() const
{
  cursor cur({location_, cu_->data().end()});
  if (form_ != DW_FORM_addr)
    error::send("Invalid address type");
  auto elf = cu_->dwarf_info()->elf_file();
  return file_addr{*elf, cur.u64()};
}

std::uint32_t mdb::attr::as_section_offset() const
{
  cursor cur({location_, cu_->data().end()});
  if (form_ != DW_FORM_sec_offset)
    error::send("Invaid offset type");
  return cur.u32();
}

std::uint64_t mdb::attr::as_int() const
{
  cursor cur({location_, cu_->data().end()});
  switch (form_)
  {
    case DW_FORM_data1:
      return cur.u8();
    case DW_FORM_data2:
      return cur.u16();
    case DW_FORM_data4:
      return cur.u32();
    case DW_FORM_data8:
      return cur.u64();
    case DW_FORM_udata:
      return cur.uleb128();
    default:
      error::send("Invalid integer type");
  }
}

mdb::span<const std::byte> mdb::attr::as_block() const
{
  std::size_t size;
  cursor      cur({location_, cu_->data().end()});
  switch (form_)
  {
    case DW_FORM_block1:
      size = cur.u8();
      break;
    case DW_FORM_block2:
      size = cur.u16();
      break;
    case DW_FORM_block4:
      size = cur.u32();
      break;
    case DW_FORM_block:
      size = cur.uleb128();
      break;
    default:
      error::send("Invalid break type");
  }

  return {cur.position(), size};
}

mdb::die mdb::attr::as_reference() const
{
  cursor      cur({location_, cu_->data().end()});
  std::size_t offset;
  switch (form_)
  {
    case DW_FORM_ref1:
      offset = cur.u8();
      break;
    case DW_FORM_ref2:
      offset = cur.u16();
      break;
    case DW_FORM_ref4:
      offset = cur.u32();
      break;
    case DW_FORM_ref8:
      offset = cur.u64();
      break;
    case DW_FORM_ref_udata:
      offset = cur.uleb128();
      break;
    case DW_FORM_ref_addr:
    {
      offset          = cur.u32();
      auto  section   = cu_->dwarf_info()->elf_file()->get_section_contents(".debug_info");
      auto  die_pos   = section.begin() + offset;
      auto& cus       = cu_->dwarf_info()->compile_units();
      auto  cu_finder = [=](auto& cu)
      { return cu->data().begin() <= die_pos and cu->data().end() > die_pos; };
      auto   cu_for_offset = std::find_if(begin(cus), end(cus), cu_finder);
      cursor ref_cur({die_pos, cu_for_offset->get()->data().end()});
      return parse_die(**cu_for_offset, ref_cur);
    }
    default:
      error::send("Invalid reference type");
  }

  cursor ref_cur({cu_->data().begin() + offset, cu_->data().end()});
  return parse_die(*cu_, ref_cur);
}

std::string_view mdb::attr::as_string() const
{
  cursor cur({location_, cu_->data().end()});
  switch (form_)
  {
    case DW_FORM_string:
      return cur.string();
    case DW_FORM_strp:
    {
      auto   offset = cur.u32();
      auto   stab   = cu_->dwarf_info()->elf_file()->get_section_contents(".debug_str");
      cursor stab_cur({stab.begin() + offset, stab.end()});
      return stab_cur.string();
    }
    default:
      error::send("Invalid string type");
  }
}

mdb::file_addr mdb::die::low_pc() const
{
  return (*this)[DW_AT_low_pc].as_address();
}

mdb::file_addr mdb::die::high_pc() const
{
  auto           attr = (*this)[DW_AT_high_pc];
  std::file_addr addr;
  if (attr.form() == DW_FORM_addr)
  {
    addr = attr.as_address();
  }
  else
  {
    addr = low_pc() + attr.as_int();
  }
  return file_addr{*cu_->dwarf_info()->elf_file(), addr};
}
