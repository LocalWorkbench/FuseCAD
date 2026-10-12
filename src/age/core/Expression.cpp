// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/core/Expression.h"

#include "age/core/Error.h"
#include "age/core/Math.h"
#include "age/core/Units.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <optional>
#include <ranges>
#include <utility>

namespace age {

struct Expression::Node {
    // Number: a bare literal. Constant: pi / e (unitless). Unit: args[0] scaled by
    // `number` into base units of kind `unit`, spelled `name`.
    enum class Kind { Number, Constant, Unit, Ident, Neg, Add, Sub, Mul, Div, Pow, Call } kind = Kind::Number;
    double number = 0;
    UnitKind unit = UnitKind::Unitless;
    std::string name;
    size_t pos = 0; // Ident: offset of the name in the source
    std::vector<std::shared_ptr<const Node>> args;
};

namespace {

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

using NodePtr = std::shared_ptr<const Expression::Node>;
using Kind = Expression::Node::Kind;

// How a function treats the kinds of its arguments (see Expression.h).
enum class Signature {
    Trig,        // angle -> unitless
    InverseTrig, // unitless -> angle
    Atan2,       // (length, length) | (unitless, unitless) -> angle
    Sqrt,        // unitless -> unitless
    Abs,         // any kind, preserved
    Rounding,    // length | unitless, preserved
    MinMax,      // two of the same kind, preserved
};

struct Function {
    std::string_view name;
    int arity;
    Signature signature;
    double (*fn)(const double*);
};

constexpr std::array<Function, 14> kFunctions{{
    {"sin", 1, Signature::Trig, [](const double* a) { return std::sin(a[0]); }},
    {"cos", 1, Signature::Trig, [](const double* a) { return std::cos(a[0]); }},
    {"tan", 1, Signature::Trig, [](const double* a) { return std::tan(a[0]); }},
    {"asin", 1, Signature::InverseTrig, [](const double* a) { return std::asin(a[0]); }},
    {"acos", 1, Signature::InverseTrig, [](const double* a) { return std::acos(a[0]); }},
    {"atan", 1, Signature::InverseTrig, [](const double* a) { return std::atan(a[0]); }},
    {"atan2", 2, Signature::Atan2, [](const double* a) { return std::atan2(a[0], a[1]); }},
    {"sqrt", 1, Signature::Sqrt, [](const double* a) { return std::sqrt(a[0]); }},
    {"abs", 1, Signature::Abs, [](const double* a) { return std::fabs(a[0]); }},
    {"floor", 1, Signature::Rounding, [](const double* a) { return std::floor(a[0]); }},
    {"ceil", 1, Signature::Rounding, [](const double* a) { return std::ceil(a[0]); }},
    {"round", 1, Signature::Rounding, [](const double* a) { return std::round(a[0]); }},
    {"min", 2, Signature::MinMax, [](const double* a) { return std::min(a[0], a[1]); }},
    {"max", 2, Signature::MinMax, [](const double* a) { return std::max(a[0], a[1]); }},
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

NodePtr makeUnit(NodePtr operand, std::string_view name, UnitInfo unit) {
    auto n = std::make_shared<Expression::Node>();
    n->kind = Kind::Unit;
    n->args = {std::move(operand)};
    n->number = unit.toBase;
    n->unit = unit.kind;
    n->name = std::string(name);
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
        return makeUnit(std::move(n), id, *unit);
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
        const size_t start = pos_;
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

        if (auto k = findConstant(name)) return makeNode(Kind::Constant, {}, *k);
        if (lookupUnit(name)) fail("unit '" + name + "' must follow a number");
        if (std::find(refs.begin(), refs.end(), name) == refs.end()) refs.push_back(name);
        auto ident = std::make_shared<Expression::Node>();
        ident->kind = Kind::Ident;
        ident->name = std::move(name);
        ident->pos = start;
        return ident;
    }

    std::string_view src_;
    size_t pos_ = 0;
};

// A value during evaluation. A bare number has no kind yet (nullopt) and adopts
// one from its context; see Expression.h.
struct Value {
    double v = 0;
    std::optional<UnitKind> kind;

    Value() = default;
    Value(double value, std::optional<UnitKind> k) : v(value), kind(k) {}
    explicit Value(Quantity q) : v(q.value), kind(q.kind) {}
};

// Display unit of each kind, which a bare number is read in when it adopts that kind.
double bareNumberScale(UnitKind kind) { return kind == UnitKind::Angle ? kPi / 180.0 : 1.0; }

Quantity adopt(const Value& a, UnitKind kind) {
    if (a.kind) return {a.v, *a.kind};
    return {a.v * bareNumberScale(kind), kind};
}

const char* describe(std::optional<UnitKind> kind) {
    if (!kind) return "number";
    switch (*kind) {
    case UnitKind::Length:
        return "a length";
    case UnitKind::Angle:
        return "an angle";
    case UnitKind::Unitless:
        return "a unitless value";
    }
    return "";
}

class Evaluator {
public:
    Evaluator(const std::string& source, const Expression::Lookup& lookup) : source_(source), lookup_(lookup) {}

    Value eval(const Expression::Node& n) const {
        switch (n.kind) {
        case Kind::Number:
            return {n.number, std::nullopt};
        case Kind::Constant:
            return {n.number, UnitKind::Unitless};
        case Kind::Unit: {
            Value a = eval(*n.args[0]);
            if (a.kind.value_or(UnitKind::Unitless) != UnitKind::Unitless)
                fail("unit '" + n.name + "' applied to " + describe(a.kind));
            return {a.v * n.number, n.unit};
        }
        case Kind::Ident:
            return Value(lookup_(n.name));
        case Kind::Neg: {
            Value a = eval(*n.args[0]);
            return {-a.v, a.kind};
        }
        case Kind::Add:
        case Kind::Sub: {
            Value a = eval(*n.args[0]), b = eval(*n.args[1]);
            unify(a, b, n.kind == Kind::Add ? "add" : "subtract");
            return {n.kind == Kind::Add ? a.v + b.v : a.v - b.v, a.kind};
        }
        case Kind::Mul:
            return multiply(eval(*n.args[0]), eval(*n.args[1]));
        case Kind::Div:
            return divide(eval(*n.args[0]), eval(*n.args[1]));
        case Kind::Pow: {
            Value base = eval(*n.args[0]);
            Quantity exp = adopt(eval(*n.args[1]), UnitKind::Unitless);
            if (exp.kind != UnitKind::Unitless)
                fail(std::string("an exponent must be unitless, not ") + describe(exp.kind));
            if (base.kind.value_or(UnitKind::Unitless) != UnitKind::Unitless)
                fail(std::string("cannot raise ") + describe(base.kind) + " to a power");
            return {std::pow(base.v, exp.value), base.kind};
        }
        case Kind::Call:
            return call(n);
        }
        return {};
    }

private:
    [[noreturn]] void fail(const std::string& msg) const { throw Error("expression \"" + source_ + "\": " + msg); }

    // Operands that must share a kind; a bare number adopts the other's.
    void unify(Value& a, Value& b, const char* verb) const {
        if (a.kind && b.kind && a.kind != b.kind)
            fail(std::string("cannot ") + verb + " " + describe(a.kind) + " and " + describe(b.kind));
        if (b.kind) a = Value(adopt(a, *b.kind));
        if (a.kind) b = Value(adopt(b, *a.kind));
    }

    Value multiply(const Value& a, const Value& b) const {
        if (!a.kind && !b.kind) return {a.v * b.v, std::nullopt};
        const Quantity x = adopt(a, UnitKind::Unitless), y = adopt(b, UnitKind::Unitless);
        if (x.kind == UnitKind::Unitless) return {x.value * y.value, y.kind};
        if (y.kind == UnitKind::Unitless) return {x.value * y.value, x.kind};
        fail(std::string("cannot multiply ") + describe(x.kind) + " by " + describe(y.kind));
    }

    Value divide(const Value& a, const Value& b) const {
        if (b.v == 0) fail("division by zero");
        if (!a.kind && !b.kind) return {a.v / b.v, std::nullopt};
        const Quantity x = adopt(a, UnitKind::Unitless), y = adopt(b, UnitKind::Unitless);
        if (y.kind == UnitKind::Unitless) return {x.value / y.value, x.kind};
        if (x.kind == y.kind) return {x.value / y.value, UnitKind::Unitless};
        fail(std::string("cannot divide ") + describe(x.kind) + " by " + describe(y.kind));
    }

    Value call(const Expression::Node& n) const {
        const Function* fn = findFunction(n.name);
        Value a = eval(*n.args[0]);
        Value b = n.args.size() > 1 ? eval(*n.args[1]) : Value{};
        auto expect = [&](bool ok, const char* what) {
            if (!ok) fail(n.name + "() expects " + what + ", not " + describe(a.kind));
        };
        switch (fn->signature) {
        case Signature::Trig: {
            const Quantity x = adopt(a, UnitKind::Angle);
            expect(x.kind == UnitKind::Angle, "an angle");
            return {fn->fn(&x.value), UnitKind::Unitless};
        }
        case Signature::InverseTrig: {
            const Quantity x = adopt(a, UnitKind::Unitless);
            expect(x.kind == UnitKind::Unitless, "a unitless value");
            return {fn->fn(&x.value), UnitKind::Angle};
        }
        case Signature::Atan2: {
            unify(a, b, "compare");
            expect(a.kind != UnitKind::Angle, "lengths or unitless values");
            const double args[2] = {a.v, b.v};
            return {fn->fn(args), UnitKind::Angle};
        }
        case Signature::Sqrt:
            expect(a.kind.value_or(UnitKind::Unitless) == UnitKind::Unitless, "a unitless value");
            return {fn->fn(&a.v), a.kind};
        case Signature::Abs:
            return {fn->fn(&a.v), a.kind};
        case Signature::Rounding:
            expect(a.kind != UnitKind::Angle, "a length or unitless value");
            return {fn->fn(&a.v), a.kind};
        case Signature::MinMax: {
            unify(a, b, "compare");
            const double args[2] = {a.v, b.v};
            return {fn->fn(args), a.kind};
        }
        }
        return {};
    }

    const std::string& source_;
    const Expression::Lookup& lookup_;
};

void collectIdentPositions(const Expression::Node& n, std::string_view name, std::vector<size_t>& out) {
    if (n.kind == Kind::Ident && n.name == name) out.push_back(n.pos);
    for (const auto& a : n.args) collectIdentPositions(*a, name, out);
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

double Expression::evaluate(UnitKind expected, const Lookup& lookup) const {
    const Quantity q = adopt(Evaluator(source_, lookup).eval(*root_), expected);
    if (q.kind != expected)
        throw Error("expression \"" + source_ + "\" is " + describe(q.kind) + " but " + describe(expected) +
                    " is expected");
    if (!std::isfinite(q.value)) throw Error("expression \"" + source_ + "\" is not a finite number");
    return q.value;
}

double Expression::evaluate(UnitKind expected) const {
    return evaluate(expected, [this](std::string_view name) -> Quantity {
        throw Error("expression \"" + source_ + "\": unknown parameter '" + std::string(name) + "'");
    });
}

Expression Expression::renamed(std::string_view from, std::string_view to) const {
    std::vector<size_t> positions;
    collectIdentPositions(*root_, from, positions);
    if (positions.empty()) return *this;
    std::sort(positions.begin(), positions.end());
    std::string out = source_;
    for (size_t pos : std::views::reverse(positions)) out.replace(pos, from.size(), to);
    return parse(out);
}

bool isReservedName(std::string_view name) { return lookupUnit(name) || findFunction(name) || findConstant(name); }

bool isValidIdentifier(std::string_view name) {
    if (name.empty() || !isIdentStart(name[0])) return false;
    return std::all_of(name.begin(), name.end(), isIdentChar);
}

} // namespace age
