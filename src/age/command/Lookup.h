// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared argument checks for the built-in commands, so every command reports
// a missing feature or a bad expression in the same words.

#include "age/core/Error.h"
#include "age/core/Units.h"
#include "age/model/Document.h"
#include "age/model/Features.h"

#include <string>
#include <utility>

namespace age::cmd {

inline const Feature& requireFeature(const Document& doc, int id) {
    const Feature* f = doc.findFeature(id);
    if (!f) throw Error("feature " + std::to_string(id) + " does not exist");
    return *f;
}

inline Feature& requireFeature(Document& doc, int id) {
    return const_cast<Feature&>(requireFeature(std::as_const(doc), id));
}

// The feature `id`, which must be of type T ("sketch", "extrude", ...).
template <class T>
T& requireFeature(Document& doc, int id) {
    Feature& f = requireFeature(doc, id);
    T* t = featureCast<T>(&f);
    if (!t) throw Error("'" + f.name + "' is not a " + featureTypeName(T::kStaticType) + " feature");
    return *t;
}

template <class T>
const T& requireFeature(const Document& doc, int id) {
    return requireFeature<T>(const_cast<Document&>(doc), id);
}

// Evaluates `expression` against the document's parameters, so a typo is
// reported by the command rather than later as a failing feature.
inline void checkExpression(const Document& doc, const char* argument, const std::string& expression, UnitKind kind) {
    try {
        doc.parameters().evaluate(expression, kind);
    } catch (const Error& e) {
        throw Error(std::string(argument) + ": " + e.what());
    }
}

// Body ids passed as -1 mean the body most recently created or modified
// before the timeline marker.
inline int resolveBody(const Document& doc, int body) {
    if (body >= 0) return body;
    const int latest = doc.latestBody();
    if (latest < 0) throw Error("there is no body to operate on");
    return latest;
}

} // namespace age::cmd
