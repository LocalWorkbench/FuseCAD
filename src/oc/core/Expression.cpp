// SPDX-License-Identifier: GPL-3.0-or-later
#include "oc/core/Expression.h"

#include "oc/core/Error.h"
#include "oc/core/Math.h"
#include "oc/core/Units.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <utility>

namespace oc {

struct Expression::Node {
    enum class Kind { Number, Ident, Neg, Add, Sub, Mul, Div, Pow, Call } kind = Kind::Number;
    double number = 0;
    std::string name;
    std::vector<std::shared_ptr<const Node>> args;
};

namespace {

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

using NodePtr = std::shared_ptr<const Expression::Node>;
using Kind = Expression::Node::Kind;

struct Function {
    std::string_view name;
    int arity;
    double (*fn)(const double*);
};

constexpr std::array<Function, 14> kFunctions{{
    {"sin", 1, [](const double* a) { return std::sin(a[0]); }},
    {"cos", 1, [](const double* a) { return std::cos(a[0]); }},
    {"tan", 1, [](const double* a) { return std::tan(a[0]); }},
    {"asin", 1, [](const double* a) { return std::asin(a[0]); }},
    {"acos", 1, [](const double* a) { return std::acos(a[0]); }},
    {"atan", 1, [](const double* a) { return std::atan(a[0]); }},
    {"atan2", 2, [](const double* a) { return std::atan2(a[0], a[1]); }},
    {"sqrt", 1, [](const double* a) { return std::sqrt(a[0]); }},
    {"abs", 1, [](const double* a) { return std::fabs(a[0]); }},
    {"floor", 1, [](const double* a) { return std::floor(a[0]); }},
    {"ceil", 1, [](const double* a) { return std::ceil(a[0]); }},
    {"round", 1, [](const double* a) { return std::round(a[0]); }},
    {"min", 2, [](const double* a) { return std::min(a[0], a[1]); }},
    {"max", 2, [](const double* a) { return std::max(a[0], a[1]); }},
}};

const Function* findFunction(std::string_view name) {
    for (const auto& f : kFunctions)
        if (f.name == name) return &f;
    return nullptr;
}

std::optional<double> findConstant(std::string_view name) {
    if (name == "pi" || name == "PI") return kPi;
    if (name == "e") return std::numbers::e;
    return std::nullopt;
}

NodePtr makeNode(Kind kind, std::vector<NodePtr> args = {}, double number = 0, std::string name = {}) {
    auto n = std::make_shared<Expression::Node>();
    n->kind = kind;
    n->args = std::move(args);
    n->number = number;
    n->name = std::move(name);
    return n;
}

class Parser {
public:
    explicit Parser(std::string_view src) : src_(src) {}

    NodePtr parseAll() {
        NodePtr n = parseExpr();
        skipSpace();
        if (pos_ != src_.size()) fail("unexpected '" + std::string(1, src_[pos_]) + "'");
        return n;
    }

    std::vector<std::string> refs;

private:
    [[noreturn]] void fail(const std::string& msg) const {
        throw Error("expression \"" + std::string(src_) + "\": " + msg + " at column " + std::to_string(pos_ + 1));
    }

    void skipSpace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) ++pos_;
    }

    bool accept(char c) {
        skipSpace();
        if (pos_ < src_.size() && src_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char c) {
        if (!accept(c)) fail(std::string("expected '") + c + "'");
    }

    std::string_view peekIdent() {
        skipSpace();
        size_t p = pos_;
        if (p >= src_.size() || !isIdentStart(src_[p])) return {};
        while (p < src_.size() && isIdentChar(src_[p])) ++p;
        return src_.substr(pos_, p - pos_);
    }

    // After a number or parenthesised group: consume a trailing unit if present.
    NodePtr maybeUnit(NodePtr n) {
        std::string_view id = peekIdent();
        if (id.empty()) return n;
        auto unit = lookupUnit(id);
        if (!unit) return n;
        // "10 mm(" is not a unit suffix; leave it for the caller to reject.
        size_t after = pos_ + id.size();
        while (after < src_.size() && std::isspace(static_cast<unsigned char>(src_[after]))) ++after;
        if (after < src_.size() && src_[after] == '(') return n;
        pos_ += id.size();
        if (unit->toBase == 1.0) return n;
        return makeNode(Kind::Mul, {n, makeNode(Kind::Number, {}, unit->toBase)});
    }

    NodePtr parseExpr() {
        NodePtr lhs = parseTerm();
        for (;;) {
            if (accept('+'))
                lhs = makeNode(Kind::Add, {lhs, parseTerm()});
            else if (accept('-'))
                lhs = makeNode(Kind::Sub, {lhs, parseTerm()});
            else
                return lhs;
        }
    }

    NodePtr parseTerm() {
        NodePtr lhs = parseUnary();
        for (;;) {
            if (accept('*'))
                lhs = makeNode(Kind::Mul, {lhs, parseUnary()});
            else if (accept('/'))
                lhs = makeNode(Kind::Div, {lhs, parseUnary()});
            else
                return lhs;
        }
    }

    NodePtr parseUnary() {
        if (accept('-')) return makeNode(Kind::Neg, {parseUnary()});
        if (accept('+')) return parseUnary();
        NodePtr base = parsePrimary();
        if (accept('^')) return makeNode(Kind::Pow, {base, parseUnary()});
        return base;
    }

    NodePtr parsePrimary() {
        skipSpace();
        if (pos_ >= src_.size()) fail("unexpected end of expression");
        char c = src_[pos_];

        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            double v = 0;
            const char* begin = src_.data() + pos_;
            const char* end = src_.data() + src_.size();
            auto [ptr, ec] = std::from_chars(begin, end, v);
            if (ec != std::errc()) fail("invalid number");
            pos_ += static_cast<size_t>(ptr - begin);
            return maybeUnit(makeNode(Kind::Number, {}, v));
        }

        if (accept('(')) {
            NodePtr n = parseExpr();
            expect(')');
            return maybeUnit(n);
        }

        std::string_view id = peekIdent();
        if (id.empty()) fail("unexpected '" + std::string(1, c) + "'");
        pos_ += id.size();
        std::string name(id);

        if (accept('(')) {
            const Function* fn = findFunction(name);
            if (!fn) fail("unknown function '" + name + "'");
            std::vector<NodePtr> args;
            if (!accept(')')) {
                do args.push_back(parseExpr());
                while (accept(','));
                expect(')');
            }
            if (std::cmp_not_equal(args.size(), fn->arity))
                fail("function '" + name + "' expects " + std::to_string(fn->arity) + " argument(s)");
            return makeNode(Kind::Call, std::move(args), 0, name);
        }

        if (auto k = findConstant(name)) return makeNode(Kind::Number, {}, *k);
        if (lookupUnit(name)) fail("unit '" + name + "' must follow a number");
        if (std::find(refs.begin(), refs.end(), name) == refs.end()) refs.push_back(name);
        return makeNode(Kind::Ident, {}, 0, name);
    }

    std::string_view src_;
    size_t pos_ = 0;
};

double eval(const Expression::Node& n, const Expression::Lookup& lookup) {
    switch (n.kind) {
    case Kind::Number:
        return n.number;
    case Kind::Ident:
        return lookup(n.name);
    case Kind::Neg:
        return -eval(*n.args[0], lookup);
    case Kind::Add:
        return eval(*n.args[0], lookup) + eval(*n.args[1], lookup);
    case Kind::Sub:
        return eval(*n.args[0], lookup) - eval(*n.args[1], lookup);
    case Kind::Mul:
        return eval(*n.args[0], lookup) * eval(*n.args[1], lookup);
    case Kind::Div: {
        double d = eval(*n.args[1], lookup);
        if (d == 0) throw Error("division by zero");
        return eval(*n.args[0], lookup) / d;
    }
    case Kind::Pow:
        return std::pow(eval(*n.args[0], lookup), eval(*n.args[1], lookup));
    case Kind::Call: {
        const Function* fn = findFunction(n.name);
        double a[2] = {};
        for (size_t i = 0; i < n.args.size(); ++i) a[i] = eval(*n.args[i], lookup);
        return fn->fn(a);
    }
    }
    return 0;
}

} // namespace

Expression::Expression() : root_(makeNode(Kind::Number)), source_("0") {}

Expression Expression::parse(std::string_view source) {
    Parser p(source);
    Expression e;
    e.root_ = p.parseAll();
    e.source_ = std::string(source);
    e.refs_ = std::move(p.refs);
    return e;
}

double Expression::evaluate(const Lookup& lookup) const {
    double v = eval(*root_, lookup);
    if (!std::isfinite(v)) throw Error("expression \"" + source_ + "\" is not a finite number");
    return v;
}

double Expression::evaluate() const {
    return evaluate([this](std::string_view name) -> double {
        throw Error("expression \"" + source_ + "\": unknown parameter '" + std::string(name) + "'");
    });
}

bool isReservedName(std::string_view name) { return lookupUnit(name) || findFunction(name) || findConstant(name); }

bool isValidIdentifier(std::string_view name) {
    if (name.empty() || !isIdentStart(name[0])) return false;
    return std::all_of(name.begin(), name.end(), isIdentChar);
}

} // namespace oc
