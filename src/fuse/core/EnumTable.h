// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace fuse {

// The usual row type for an EnumTable: an enumerator and its stable name.
template <class E>
struct EnumName {
    E value;
    const char* name;
};

namespace detail {
// Deliberately not constexpr: reaching it during constant evaluation is a compile error.
inline void enumTableEntriesMustFollowEnumOrder() {}
} // namespace detail

// Two-way mapping between an enum and stable string names (file format keys).
// `Entry` is any aggregate with `value` and `name` members; extra columns are
// allowed and reachable through operator[]. Rows must list every enumerator in
// declaration order, which is checked at compile time, so lookup by value is
// a plain index with no fallback.
template <class Entry, std::size_t N>
class EnumTable {
public:
    using Enum = decltype(Entry::value);

    consteval explicit EnumTable(const std::array<Entry, N>& entries) : entries_(entries) {
        for (std::size_t i = 0; i < N; ++i)
            if (static_cast<std::size_t>(entries_[i].value) != i) detail::enumTableEntriesMustFollowEnumOrder();
    }

    constexpr const Entry& operator[](Enum value) const { return entries_[static_cast<std::size_t>(value)]; }
    constexpr const std::array<Entry, N>& entries() const { return entries_; }
    constexpr const char* name(Enum value) const { return (*this)[value].name; }
    constexpr std::optional<Enum> parse(std::string_view name) const {
        for (const Entry& e : entries_)
            if (name == e.name) return e.value;
        return std::nullopt;
    }

private:
    std::array<Entry, N> entries_;
};

// makeEnumTable<EnumName<Color>>({{Color::Red, "red"}, {Color::Green, "green"}})
//
// Enums that appear in files or command arguments publish their table through
// an `enumTable(E)` function next to the enum, found by argument-dependent lookup.
template <class Entry, std::size_t N>
consteval EnumTable<Entry, N> makeEnumTable(const Entry (&entries)[N]) {
    std::array<Entry, N> a{};
    for (std::size_t i = 0; i < N; ++i) a[i] = entries[i];
    return EnumTable<Entry, N>(a);
}

} // namespace fuse
