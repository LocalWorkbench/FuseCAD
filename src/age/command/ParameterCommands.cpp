// SPDX-License-Identifier: GPL-3.0-or-later
#include "age/command/ParameterCommands.h"

namespace age::cmd {

NoOutput AddParameter::run(EditContext& ctx) const {
    ctx.doc.parameters().add(name, expression, unit, comment);
    return {};
}

NoOutput SetParameter::run(EditContext& ctx) const {
    ctx.doc.parameters().setExpression(name, expression);
    return {};
}

NoOutput RenameParameter::run(EditContext& ctx) const {
    ctx.doc.renameParameter(name, newName);
    return {};
}

NoOutput RemoveParameter::run(EditContext& ctx) const {
    ctx.doc.removeParameter(name);
    return {};
}

} // namespace age::cmd
