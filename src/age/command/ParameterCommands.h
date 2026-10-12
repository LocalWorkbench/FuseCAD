// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "age/command/Command.h"

#include <string>

namespace age::cmd {

struct AddParameter {
    static constexpr CommandInfo info{
        "parameter_add", "Add Parameter",
        "Add a user parameter. Its expression may carry units and reference other parameters, e.g. \"40 mm\" "
        "or \"width / 2\". Features and sketch dimensions can then use the parameter by name."};
    std::string name;
    std::string expression;
    UnitKind unit = UnitKind::Length;
    std::string comment;
    static constexpr auto fields = std::tuple{
        field("name", &AddParameter::name, "Unique name: a letter or underscore, then letters, digits or underscores"),
        field("expression", &AddParameter::expression, "The parameter's value", Format::Expression),
        defaulted("unit", &AddParameter::unit, "What the expression must evaluate to"),
        defaulted("comment", &AddParameter::comment, "A note shown next to the parameter"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct SetParameter {
    static constexpr CommandInfo info{"parameter_set", "Change Parameter",
                                      "Change a parameter's expression. Everything that uses it is recomputed."};
    std::string name;
    std::string expression;
    static constexpr auto fields = std::tuple{
        field("name", &SetParameter::name, "The parameter to change"),
        field("expression", &SetParameter::expression, "Its new value", Format::Expression),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct RenameParameter {
    static constexpr CommandInfo info{
        "parameter_rename", "Rename Parameter",
        "Rename a parameter and update every expression, feature and sketch dimension that uses it."};
    std::string name;
    std::string newName;
    static constexpr auto fields = std::tuple{
        field("name", &RenameParameter::name, "The parameter to rename"),
        field("new_name", &RenameParameter::newName, "Its new, unique name"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

struct RemoveParameter {
    static constexpr CommandInfo info{"parameter_remove", "Delete Parameter",
                                      "Delete a parameter. Refused while anything still uses it."};
    std::string name;
    static constexpr auto fields = std::tuple{
        field("name", &RemoveParameter::name, "The parameter to delete"),
    };
    using Output = NoOutput;
    Output run(EditContext& ctx) const;
};

} // namespace age::cmd
