// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Typed command arguments and results <-> JSON, plus the JSON Schema that
// describes them. A struct lists its members once, in a `fields` tuple; JSON
// parsing, validation messages, schema generation and serialisation all derive
// from that list, so what agents are told always matches what the code reads.
//
//   struct Move {
//       int point = 0;
//       Vec2 to;
//       static constexpr auto fields = std::tuple{
//           field("point", &Move::point, "Id of the point to move", Format::EntityId),
//           field("to", &Move::to, "New position, in mm"),
//       };
//   };

#include "age/core/Error.h"
#include "age/core/Math.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace age::cmd {

using nlohmann::json;

// Malformed command arguments. The message names the offending field.
class ArgumentError : public Error {
public:
    using Error::Error;
};

// What a value means beyond its JSON type; emitted as the schema's "format".
enum class Format {
    None,
    Expression,   // any expression: "40 mm", "width / 2", "4"
    Length,       // expression that evaluates to a length
    Angle,        // expression that evaluates to an angle
    FeatureId,    // id of a feature on the timeline
    BodyId,       // id of a body (the id of the feature that created it)
    EntityId,     // id of an entity within one sketch
    ConstraintId, // id of a constraint within one sketch
    FilePath,
};

const char* formatName(Format f);

template <class Owner, class T>
struct Field {
    const char* name;
    T Owner::* member;
    const char* description;
    Format format;
    bool required;
};

template <class T>
inline constexpr bool kIsOptional = false;
template <class T>
inline constexpr bool kIsOptional<std::optional<T>> = true;

// A field the caller must supply. std::optional members may always be omitted.
template <class Owner, class T>
constexpr Field<Owner, T> field(const char* name, T Owner::* member, const char* description,
                                Format format = Format::None) {
    return {name, member, description, format, !kIsOptional<T>};
}

// A field that may be omitted, keeping its default member initialiser (which
// the schema advertises as "default").
template <class Owner, class T>
constexpr Field<Owner, T> defaulted(const char* name, T Owner::* member, const char* description,
                                    Format format = Format::None) {
    return {name, member, description, format, false};
}

// FieldsOf<T>::fields is T::fields, or a specialisation for types that cannot
// carry their own list (such as model types that know nothing of commands).
template <class T>
struct FieldsOf {};
template <class T>
    requires requires { T::fields; }
struct FieldsOf<T> {
    static constexpr const auto& fields = T::fields;
};

template <class T>
concept Described = requires { FieldsOf<T>::fields; };

// Enums publish their names through an `enumTable(E)` function (EnumTable.h).
template <class E>
concept NamedEnum = std::is_enum_v<E> && requires(E e) { enumTable(e); };

template <class T>
struct Codec; // read(json, path) -> T, write(T) -> json, schema() -> json

namespace detail {

[[noreturn]] inline void mismatch(const std::string& path, const char* expected) {
    throw ArgumentError("'" + path + "' must be " + expected);
}

inline std::string child(const std::string& path, const char* name) {
    return path.empty() ? std::string(name) : path + "." + name;
}

inline json numberArraySchema(int size) {
    return {{"type", "array"}, {"items", {{"type", "number"}}}, {"minItems", size}, {"maxItems", size}};
}

inline double number(const json& j, const std::string& path) {
    if (!j.is_number()) mismatch(path, "a number");
    return j.get<double>();
}

} // namespace detail

template <>
struct Codec<bool> {
    static json schema() { return {{"type", "boolean"}}; }
    static bool read(const json& j, const std::string& path) {
        if (!j.is_boolean()) detail::mismatch(path, "true or false");
        return j.get<bool>();
    }
    static json write(bool v) { return v; }
};

template <>
struct Codec<int> {
    static json schema() { return {{"type", "integer"}}; }
    static int read(const json& j, const std::string& path) {
        if (!j.is_number_integer()) detail::mismatch(path, "an integer");
        const auto v = j.get<long long>();
        if (v < std::numeric_limits<int>::min() || v > std::numeric_limits<int>::max())
            detail::mismatch(path, "an integer in range");
        return static_cast<int>(v);
    }
    static json write(int v) { return v; }
};

template <>
struct Codec<double> {
    static json schema() { return {{"type", "number"}}; }
    static double read(const json& j, const std::string& path) { return detail::number(j, path); }
    static json write(double v) { return v; }
};

template <>
struct Codec<std::string> {
    static json schema() { return {{"type", "string"}}; }
    static std::string read(const json& j, const std::string& path) {
        if (!j.is_string()) detail::mismatch(path, "a string");
        return j.get<std::string>();
    }
    static json write(const std::string& v) { return v; }
};

// Raw JSON, for results that pass a model representation through unchanged.
template <>
struct Codec<json> {
    static json schema() { return json::object(); }
    static json read(const json& j, const std::string&) { return j; }
    static json write(const json& v) { return v; }
};

template <>
struct Codec<Vec2> {
    static json schema() { return detail::numberArraySchema(2); }
    static Vec2 read(const json& j, const std::string& path) {
        if (!j.is_array() || j.size() != 2) detail::mismatch(path, "an [x, y] array");
        return {detail::number(j[0], path + "[0]"), detail::number(j[1], path + "[1]")};
    }
    static json write(Vec2 v) { return json::array({v.x, v.y}); }
};

template <>
struct Codec<Vec3> {
    static json schema() { return detail::numberArraySchema(3); }
    static Vec3 read(const json& j, const std::string& path) {
        if (!j.is_array() || j.size() != 3) detail::mismatch(path, "an [x, y, z] array");
        return {detail::number(j[0], path + "[0]"), detail::number(j[1], path + "[1]"),
                detail::number(j[2], path + "[2]")};
    }
    static json write(Vec3 v) { return json::array({v.x, v.y, v.z}); }
};

template <class T>
struct Codec<std::vector<T>> {
    static json schema() { return {{"type", "array"}, {"items", Codec<T>::schema()}}; }
    static std::vector<T> read(const json& j, const std::string& path) {
        if (!j.is_array()) detail::mismatch(path, "an array");
        std::vector<T> out;
        out.reserve(j.size());
        for (std::size_t i = 0; i < j.size(); ++i)
            out.push_back(Codec<T>::read(j[i], path + "[" + std::to_string(i) + "]"));
        return out;
    }
    static json write(const std::vector<T>& v) {
        json out = json::array();
        for (const T& e : v) out.push_back(Codec<T>::write(e));
        return out;
    }
};

// Absent and null both mean "not given"; struct writers omit empty optionals.
template <class T>
struct Codec<std::optional<T>> {
    static json schema() { return Codec<T>::schema(); }
    static std::optional<T> read(const json& j, const std::string& path) {
        if (j.is_null()) return std::nullopt;
        return Codec<T>::read(j, path);
    }
    static json write(const std::optional<T>& v) { return v ? Codec<T>::write(*v) : json(); }
};

template <NamedEnum E>
struct Codec<E> {
    static const auto& table() { return enumTable(E{}); }
    static json schema() {
        json names = json::array();
        for (const auto& e : table().entries()) names.push_back(e.name);
        return {{"type", "string"}, {"enum", names}};
    }
    static E read(const json& j, const std::string& path) {
        if (j.is_string())
            if (auto v = table().parse(j.get<std::string>())) return *v;
        std::string names;
        for (const auto& e : table().entries()) names += (names.empty() ? "" : ", ") + std::string(e.name);
        throw ArgumentError("'" + path + "' must be one of: " + names);
    }
    static json write(E v) { return table().name(v); }
};

template <Described T>
struct Codec<T> {
    static constexpr const auto& fields() { return FieldsOf<T>::fields; }

    static json schema() {
        json properties = json::object();
        json required = json::array();
        const T defaults{};
        std::apply([&](const auto&... f) { (addProperty(properties, required, defaults, f), ...); }, fields());
        json s{{"type", "object"}, {"properties", properties}, {"additionalProperties", false}};
        if (!required.empty()) s["required"] = required;
        return s;
    }

    static T read(const json& j, const std::string& path) {
        if (!j.is_object()) {
            if (path.empty()) throw ArgumentError("arguments must be a JSON object");
            detail::mismatch(path, "an object");
        }
        for (const auto& [key, value] : j.items()) {
            const bool known = std::apply([&](const auto&... f) { return ((key == f.name) || ...); }, fields());
            if (!known) throw ArgumentError("unknown argument '" + detail::child(path, key.c_str()) + "'");
        }
        T out{};
        std::apply([&](const auto&... f) { (readField(j, out, f, path), ...); }, fields());
        return out;
    }

    static json write(const T& v) {
        json out = json::object();
        std::apply(
            [&](const auto&... f) {
                (
                    [&] {
                        json value = Codec<std::remove_cvref_t<decltype(v.*f.member)>>::write(v.*f.member);
                        if (!value.is_null()) out[f.name] = std::move(value);
                    }(),
                    ...);
            },
            fields());
        return out;
    }

private:
    template <class Owner, class M>
    static void addProperty(json& properties, json& required, const T& defaults, const Field<Owner, M>& f) {
        json s = Codec<M>::schema();
        s["description"] = f.description;
        if (f.format != Format::None) s["format"] = formatName(f.format);
        if (f.required)
            required.push_back(f.name);
        else if constexpr (!kIsOptional<M>)
            s["default"] = Codec<M>::write(defaults.*f.member);
        properties[f.name] = std::move(s);
    }

    template <class Owner, class M>
    static void readField(const json& j, T& out, const Field<Owner, M>& f, const std::string& path) {
        const std::string fieldPath = detail::child(path, f.name);
        auto it = j.find(f.name);
        if (it == j.end() || it->is_null()) {
            if (f.required) throw ArgumentError("missing argument '" + fieldPath + "'");
            return;
        }
        out.*f.member = Codec<M>::read(*it, fieldPath);
    }
};

} // namespace age::cmd
