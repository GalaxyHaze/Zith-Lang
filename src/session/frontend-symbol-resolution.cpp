#include "session/frontend-context-internal.hpp"
#include "session/frontend-context.hpp"

#include "diagnostics/error-codes.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zith::session {

std::vector<MergedSymbol>
FrontendContext::mergeSymbols(const std::vector<ModuleArtifactPtr> &modules) {
    std::vector<MergedSymbol> result;
    for (const auto &module : modules) {
        auto append = [&](const LocalSymbolInfo &symbol) {
            result.push_back({
                symbol.name,
                symbol.visibility,
                symbol.kind,
                ModuleSymbolRef{module->key, symbol.id},
                symbol.span,
            });
        };
        for (const auto &symbol : module->publicSymbols)
            append(symbol);
        for (const auto &symbol : module->moduleSymbols)
            append(symbol);
    }
    std::sort(result.begin(), result.end(),
              [](const MergedSymbol &left, const MergedSymbol &right) {
                  if (left.name != right.name)
                      return left.name < right.name;
                  if (left.origin.module != right.origin.module)
                      return left.origin.module < right.origin.module;
                  return left.origin.localSymbol.value < right.origin.localSymbol.value;
              });
    return result;
}

const ResolvedName *lookupExprResolution(const ModuleResolution &resolution,
                                         frontend::ExprId expr) noexcept {
    if (!expr)
        return nullptr;
    const auto begin = resolution.expressions.begin();
    const auto end   = resolution.expressions.end();
    const auto found = std::lower_bound(
        begin, end, expr.value,
        [](const ResolvedName &entry, const uint32_t value) { return entry.expr.value < value; });
    if (found == end || found->expr != expr || found->name.empty())
        return nullptr;
    return &*found;
}

const ResolvedName *lookupBinding(const ModuleResolution &resolution, std::string_view name,
                                  frontend::ScopeId from,
                                  const std::vector<frontend::Scope> &scopes) noexcept {
    const ResolvedName *result = nullptr;
    frontend::ScopeId current  = from;
    bool exhausted             = false;
    while (result == nullptr && !exhausted) {
        for (const auto &binding : resolution.bindings) {
            if (binding.scope == current && binding.name == name) {
                result = &binding;
                break;
            }
        }
        if (result != nullptr)
            break;
        if (!current) {
            exhausted = true;
            break;
        }
        if (current.value > scopes.size()) {
            current = frontend::ScopeId{};
            continue;
        }
        current = scopes[current.value - 1U].parent;
    }
    return result;
}

std::vector<const ResolvedName *> lookupOverloads(const ModuleResolution &resolution,
                                                  std::string_view name, frontend::ScopeId from,
                                                  const std::vector<frontend::Scope> &scopes) {
    std::vector<const ResolvedName *> result;
    frontend::ScopeId current = from;
    for (unsigned guard = 0; guard < 256U; ++guard) {
        for (const auto &binding : resolution.bindings) {
            if (binding.scope == current && binding.name == name)
                result.push_back(&binding);
        }
        // The nearest scope that declares the name wins outright: an inner
        // declaration shadows an outer overload set rather than extending it.
        if (!result.empty() || !current)
            break;
        if (current.value > scopes.size()) {
            current = frontend::ScopeId{};
            continue;
        }
        current = scopes[current.value - 1U].parent;
    }
    return result;
}

std::vector<ModuleResolution>
FrontendContext::buildResolutions(const std::vector<ModuleArtifactPtr> &modules,
                                  const std::vector<ImportEdge> &import_graph,
                                  std::vector<ModuleDiagnostic> &diagnostics) {
    memory::FlatMap<ModuleKey, const ModuleArtifact *> module_by_key;
    for (const auto &module : modules)
        module_by_key.insert(module->key, module.get());

    std::vector<ModuleResolution> result;
    result.reserve(modules.size());
    for (const auto &module : modules) {
        ModuleResolution resolution;
        resolution.module = module->key;
        // `export path` re-exports a dependency to every consumer of this
        // module. Collect the transitive exported imports so each module sees
        // both its own imports and those forwarded by its direct dependencies.
        std::vector<const ImportEdge *> effective_edges;
        std::vector<ModuleKey> pending_export_roots{module->key};
        memory::FlatSet<ModuleKey> visited_export_roots;
        while (!pending_export_roots.empty()) {
            const auto export_root = pending_export_roots.back();
            pending_export_roots.pop_back();
            if (!visited_export_roots.insert(export_root))
                continue;
            for (const auto &edge : import_graph) {
                if (edge.importer != export_root)
                    continue;
                if (export_root != module->key && !edge.request.isExport)
                    continue;
                effective_edges.push_back(&edge);
                const auto *target_artifact =
                    module_by_key.get(edge.targets.empty() ? ModuleKey{} : edge.targets.front());
                if (target_artifact)
                    pending_export_roots.push_back(edge.targets.empty() ? ModuleKey{}
                                                                        : edge.targets.front());
            }
        }

        // Bindings are keyed by (scope, name): a name only conflicts with another
        // binding declared in the *same* scope.  The module scope is ScopeId{}.
        std::map<std::pair<uint32_t, std::string>, std::vector<size_t>> bindings;
        auto add_binding = [&](ResolvedName binding, const frontend::ScopeId scope) {
            binding.scope  = scope;
            const auto key = std::make_pair(scope.value, binding.name);
            auto &slots    = bindings[key];
            for (const auto index : slots) {
                const auto &existing = resolution.bindings[index];
                // Two `fn` declarations may share a name (overloading) as long as
                // neither is `extern` (the C ABI fixes one linkage name) and their
                // parameter types differ after stripping memory qualifiers.
                const bool both_fn = existing.declKind == frontend::DeclKind::Function &&
                                     binding.declKind == frontend::DeclKind::Function;
                if (both_fn && !existing.isExtern && !binding.isExtern &&
                    existing.signature != binding.signature) {
                    continue;
                }
                diagnostics.push_back({diagnostics::Severity::Error,
                                       diagnostics::err::DuplicateDecl,
                                       "duplicate binding '" + binding.name + "' in this scope",
                                       module->fileId, binding.span.start, binding.span.end});
                return;
            }
            slots.push_back(resolution.bindings.size());
            resolution.bindings.push_back(std::move(binding));
        };

        for (const auto &declaration : module->frontend->declarations()) {
            if (declaration.kind == frontend::DeclKind::Import ||
                declaration.kind == frontend::DeclKind::Error || declaration.name.empty() ||
                declaration.parentScope)
                continue;
            // Macros live in their own namespace, resolved during expansion.
            if (declaration.kind == frontend::DeclKind::Macro)
                continue;
            // Method declarations are not module names: struct/trait/interface
            // members are resolved through the owner's lookup table instead of
            // the module scope. Their parameters must still be registered so
            // the method body can resolve `self` and other named parameters.
            if (declaration.ownerName.empty()) {
                ResolvedName decl_binding{declaration.name,
                                          ResolutionKind::Declaration,
                                          declaration.span,
                                          {module->key, frontend::SymbolId{declaration.id.value}},
                                          declaration.id,
                                          {},
                                          {}};
                decl_binding.declKind        = declaration.kind;
                decl_binding.isExtern        = declaration.isExtern;
                decl_binding.externalSymbol  = declaration.externalSymbol;
                decl_binding.isVariadic      = declaration.isVariadic;
                decl_binding.isVariadicSlice = !declaration.parameters.empty() &&
                                               declaration.parameters.back().isVariadicSlice;
                if (declaration.kind == frontend::DeclKind::Variable)
                    decl_binding.bindingKind = declaration.bindingKind;
                if (declaration.kind == frontend::DeclKind::Function) {
                    decl_binding.signature =
                        frontend::functionSignature(*module->frontend, declaration);
                }
                add_binding(std::move(decl_binding), frontend::ScopeId{});
            }
            // Parameters live in the scope of the function body block, so two
            // functions may reuse the same parameter name.
            frontend::ScopeId parameter_scope;
            if (declaration.body &&
                declaration.body.value <= module->frontend->expressions().size()) {
                parameter_scope =
                    module->frontend->expressions()[declaration.body.value - 1U].scope;
            }
            // Struct fields, enum variants, interface members, union fields, and
            // parameters of other composites live in the type's own lookup table,
            // not in the module resolution scope.
            const bool params_are_bindings = declaration.kind == frontend::DeclKind::Function;
            if (params_are_bindings && declaration.body) {
                for (const auto &parameter : declaration.parameters) {
                    ResolvedName parameter_binding{parameter.name,
                                                   ResolutionKind::Declaration,
                                                   parameter.span,
                                                   {},
                                                   {},
                                                   parameter.id,
                                                   {}};
                    parameter_binding.declKind    = declaration.kind;
                    parameter_binding.bindingKind = parameter.bindingKind;
                    add_binding(std::move(parameter_binding), parameter_scope);
                }
            }
        }
        // Local `state` declarations are visible only from the body scope that
        // contains them. Binding them outside the loop above keeps them out of
        // the module scope and lets overloads reuse the same duplicate rules.
        for (const auto &declaration : module->frontend->declarations()) {
            if (declaration.kind != frontend::DeclKind::Function || !declaration.parentScope ||
                declaration.name.empty())
                continue;
            ResolvedName local_state{
                declaration.name,       ResolutionKind::Declaration,
                declaration.span,       {module->key, frontend::SymbolId{declaration.id.value}},
                declaration.id,         {},
                declaration.parentScope};
            local_state.declKind       = declaration.kind;
            local_state.isExtern       = declaration.isExtern;
            local_state.externalSymbol = declaration.externalSymbol;
            local_state.isVariadic     = declaration.isVariadic;
            local_state.isVariadicSlice =
                !declaration.parameters.empty() && declaration.parameters.back().isVariadicSlice;
            local_state.signature = frontend::functionSignature(*module->frontend, declaration);
            add_binding(std::move(local_state), declaration.parentScope);
            if (declaration.body &&
                declaration.body.value <= module->frontend->expressions().size()) {
                const auto parameter_scope =
                    module->frontend->expressions()[declaration.body.value - 1U].scope;
                for (const auto &parameter : declaration.parameters) {
                    ResolvedName parameter_binding{parameter.name,
                                                   ResolutionKind::Declaration,
                                                   parameter.span,
                                                   {},
                                                   {},
                                                   parameter.id,
                                                   {}};
                    parameter_binding.declKind = declaration.kind;
                    add_binding(std::move(parameter_binding), parameter_scope);
                }
            }
        }
        // Local bindings take the scope of the block that contains them; the
        // statement list itself carries no scope information.
        memory::FlatMap<uint32_t, frontend::ScopeId> statement_scopes;
        for (const auto &expression : module->frontend->expressions()) {
            if (expression.kind != frontend::ExprKind::Block)
                continue;
            for (const auto statement_id : expression.statements) {
                if (statement_id)
                    statement_scopes.insert(statement_id.value, expression.scope);
            }
        }
        for (const auto &statement : module->frontend->statements()) {
            // Bindings inside a macro template are not real declarations.
            if (module->frontend->isMacroTemplateStmt(statement.id))
                continue;
            if (statement.kind == frontend::StmtKind::Binding && !statement.binding.name.empty()) {
                frontend::ScopeId statement_scope;
                if (const auto *found = statement_scopes.get(statement.id.value)) {
                    statement_scope = *found;
                }
                ResolvedName local_binding{statement.binding.name,
                                           ResolutionKind::Declaration,
                                           statement.binding.span,
                                           {},
                                           {},
                                           statement.binding.id,
                                           {}};
                local_binding.declKind    = frontend::DeclKind::Variable;
                local_binding.bindingKind = statement.binding.bindingKind;
                add_binding(std::move(local_binding), statement_scope);
            }
        }

        for (const auto *edge_ptr : effective_edges) {
            const auto &edge = *edge_ptr;
            if (!edge.error.empty())
                continue;
            if (edge.targetKind == ImportTargetKind::CHeader) {
                if (edge.cHeader == nullptr)
                    continue;
                for (const auto &function : edge.cHeader->functions) {
                    ResolvedName foreign_binding{function.name,
                                                 ResolutionKind::Foreign,
                                                 edge.request.pathSpan,
                                                 {},
                                                 {},
                                                 {},
                                                 {},
                                                 &function};
                    foreign_binding.declKind   = frontend::DeclKind::Function;
                    foreign_binding.isExtern   = true;
                    foreign_binding.isVariadic = function.isVariadic;
                    add_binding(std::move(foreign_binding), frontend::ScopeId{});
                }
                for (const auto &constant : edge.cHeader->constants) {
                    ResolvedName foreign_constant{constant.name,
                                                  ResolutionKind::Foreign,
                                                  edge.request.pathSpan,
                                                  {},
                                                  {},
                                                  {},
                                                  {}};
                    foreign_constant.declKind        = frontend::DeclKind::Variable;
                    foreign_constant.bindingKind     = frontend::BindingKind::Const;
                    foreign_constant.foreignConstant = &constant;
                    add_binding(std::move(foreign_constant), frontend::ScopeId{});
                }
                if (!edge.request.alias.empty()) {
                    add_binding({edge.request.alias,
                                 ResolutionKind::ModuleAlias,
                                 edge.request.aliasSpan,
                                 {},
                                 {},
                                 {},
                                 {},
                                 {}},
                                frontend::ScopeId{});
                }
                continue;
            }
            const auto default_name =
                edge.request.path.empty() ? std::string{} : edge.request.path.front();
            if (!edge.request.alias.empty()) {
                ResolvedName alias;
                alias.name       = edge.request.alias;
                alias.kind       = ResolutionKind::ModuleAlias;
                alias.span       = edge.request.aliasSpan;
                alias.target     = {edge.targets.empty() ? ModuleKey{} : edge.targets.front(), {}};
                alias.modulePath = edge.request.path;
                add_binding(std::move(alias), frontend::ScopeId{});
            }
            if (!edge.request.selectors.empty()) {
                for (const auto &selector : edge.request.selectors) {
                    bool found = false;
                    for (const auto &target : edge.targets) {
                        const auto *target_artifact = module_by_key.get(target);
                        if (!target_artifact)
                            continue;
                        for (const auto &symbol : (*target_artifact)->publicSymbols) {
                            if (symbol.name != selector.name)
                                continue;
                            ResolvedName imported{selector.alias.empty() ? selector.name
                                                                         : selector.alias,
                                                  ResolutionKind::Import,
                                                  selector.span,
                                                  {target, symbol.id},
                                                  {},
                                                  {},
                                                  {}};
                            imported.declKind        = symbol.kind;
                            imported.signature       = symbol.signature;
                            imported.isExtern        = symbol.isExtern;
                            imported.externalSymbol  = symbol.externalSymbol;
                            imported.isVariadic      = symbol.isVariadic;
                            imported.isVariadicSlice = symbol.isVariadicSlice;
                            add_binding(std::move(imported), frontend::ScopeId{});
                            found = true;
                            break;
                        }
                        if (found)
                            break;
                    }
                    if (!found) {
                        diagnostics.push_back(
                            {diagnostics::Severity::Error, diagnostics::err::ImportError,
                             "import selector '" + selector.name + "' was not found in '" +
                                 edge.request.importKey() + "'",
                             module->fileId, selector.span.start, selector.span.end});
                    }
                }
                continue;
            }
            if (edge.request.isFrom) {
                for (const auto &target : edge.targets) {
                    const auto *target_artifact = module_by_key.get(target);
                    if (!target_artifact)
                        continue;
                    for (const auto &symbol : (*target_artifact)->publicSymbols) {
                        ResolvedName imported{symbol.name,
                                              ResolutionKind::Import,
                                              edge.request.span,
                                              {target, symbol.id},
                                              {},
                                              {},
                                              {}};
                        imported.declKind        = symbol.kind;
                        imported.signature       = symbol.signature;
                        imported.isExtern        = symbol.isExtern;
                        imported.externalSymbol  = symbol.externalSymbol;
                        imported.isVariadic      = symbol.isVariadic;
                        imported.isVariadicSlice = symbol.isVariadicSlice;
                        add_binding(std::move(imported), frontend::ScopeId{});
                    }
                }
            } else if (edge.request.alias.empty() && !default_name.empty()) {
                // `import Path` is a namespace binding for the full dotted
                // path (`std.counter.Counter`), never a bare last-segment
                // injection (`Counter`). Consumers therefore cannot reach
                // symbols until the path is written.
                ResolvedName alias;
                alias.name       = default_name;
                alias.kind       = ResolutionKind::ModuleAlias;
                alias.span       = edge.request.pathSpan;
                alias.target     = {edge.targets.empty() ? ModuleKey{} : edge.targets.front(), {}};
                alias.modulePath = edge.request.path;
                add_binding(std::move(alias), frontend::ScopeId{});
            }
        }

        memory::FlatSet<uint32_t> field_operand_ids;
        for (const auto &expression : module->frontend->expressions()) {
            if (expression.kind == frontend::ExprKind::Field && !expression.operands.empty())
                field_operand_ids.insert(expression.operands[0].value);
        }
        for (const auto &expression : module->frontend->expressions()) {
            if (module->frontend->isMacroTemplateExpr(expression.id))
                continue;
            if (expression.kind == frontend::ExprKind::Field &&
                field_operand_ids.contains(expression.id.value))
                continue;
            if (expression.kind == frontend::ExprKind::StructLiteral &&
                expression.text.find('.') != std::string::npos) {
                // Qualified struct literals keep their full dotted name in
                // `text` (e.g. `std.counter.Counter{...}`). Resolve through
                // the module alias just like a Field chain so sema can fetch
                // the concrete declaration from its module.
                const auto dot       = expression.text.find('.');
                const auto root_name = std::string_view(expression.text).substr(0, dot);
                const auto *alias    = lookupBinding(resolution, root_name, expression.scope,
                                                     module->frontend->scopes());
                if (alias == nullptr || alias->kind != ResolutionKind::ModuleAlias)
                    continue;
                std::vector<std::string> segments;
                size_t cursor = 0;
                while (cursor < expression.text.size()) {
                    const auto next = expression.text.find('.', cursor);
                    segments.push_back(expression.text.substr(
                        cursor, next == std::string::npos ? std::string::npos : next - cursor));
                    if (next == std::string::npos)
                        break;
                    cursor = next + 1U;
                }
                const auto target    = alias->target.module;
                const auto *artifact = module_by_key.get(target);
                if (!artifact || segments.size() < 2U)
                    continue;
                bool found = false;
                for (const auto &symbol : (*artifact)->publicSymbols) {
                    if (symbol.name != segments.back())
                        continue;
                    ResolvedName member;
                    member.name            = symbol.name;
                    member.kind            = ResolutionKind::Import;
                    member.span            = expression.span;
                    member.target          = {target, symbol.id};
                    member.expr            = expression.id;
                    member.declKind        = symbol.kind;
                    member.signature       = symbol.signature;
                    member.isExtern        = symbol.isExtern;
                    member.externalSymbol  = symbol.externalSymbol;
                    member.isVariadic      = symbol.isVariadic;
                    member.isVariadicSlice = symbol.isVariadicSlice;
                    member.modulePath      = segments;
                    resolution.expressions.push_back(std::move(member));
                    found = true;
                    break;
                }
                if (!found)
                    diagnostics.push_back({diagnostics::Severity::Error, diagnostics::err::NoMember,
                                           "qualified literal '" + expression.text +
                                               "' has no public member '" + segments.back() + "'",
                                           module->fileId, expression.span.start,
                                           expression.span.end});
            }
            // Enum discriminant expressions may reference earlier variants by bare
            // name (`PREV = SHIFT + 1`). The variants live in the enum's own lookup
            // table rather than the module scope, so resolve those occurrences to the
            // containing enum declaration here.
            if (expression.kind == frontend::ExprKind::Name && expression.scope) {
                const frontend::Declaration *variant_enum = nullptr;
                for (const auto &enum_decl : module->frontend->declarations()) {
                    if (enum_decl.kind != frontend::DeclKind::Enum)
                        continue;
                    bool inside_enum_default = false;
                    for (const auto &variant : enum_decl.parameters) {
                        if (!variant.defaultValue ||
                            variant.defaultValue.value > module->frontend->expressions().size())
                            continue;
                        const auto &default_expr =
                            module->frontend->expressions()[variant.defaultValue.value - 1U];
                        if (expression.span.start >= default_expr.span.start &&
                            expression.span.end <= default_expr.span.end) {
                            inside_enum_default = true;
                            break;
                        }
                    }
                    if (!inside_enum_default)
                        continue;
                    for (const auto &variant : enum_decl.parameters) {
                        if (variant.name == expression.text) {
                            variant_enum = &enum_decl;
                            break;
                        }
                    }
                    if (variant_enum != nullptr)
                        break;
                }
                if (variant_enum != nullptr) {
                    ResolvedName variant_binding{
                        variant_enum->name,
                        ResolutionKind::Declaration,
                        expression.span,
                        {module->key, frontend::SymbolId{variant_enum->id.value}},
                        variant_enum->id,
                        {},
                        expression.scope};
                    variant_binding.expr     = expression.id;
                    variant_binding.declKind = frontend::DeclKind::Enum;
                    resolution.expressions.push_back(std::move(variant_binding));
                }
            }
            if (expression.kind == frontend::ExprKind::Field && !expression.operands.empty()) {
                // `console.println`, `std.counter.Counter`, or `out.println`
                // where the base is a module alias. Fully-qualified paths whose
                // middle segments are also module aliases keep resolving down
                // the chain; the final field binds to the imported symbol so
                // sema/lowering never see an alias name.
                std::vector<const frontend::Expression *> chain;
                const frontend::Expression *cursor = &expression;
                while (cursor->kind == frontend::ExprKind::Field && !cursor->operands.empty()) {
                    chain.push_back(cursor);
                    if (cursor->operands[0].value > module->frontend->expressions().size())
                        break;
                    cursor = &module->frontend->expressions()[cursor->operands[0].value - 1U];
                }
                if (chain.empty() || cursor->kind != frontend::ExprKind::Name)
                    continue;
                const frontend::Expression &root = *cursor;
                const auto *alias =
                    lookupBinding(resolution, root.text, root.scope, module->frontend->scopes());
                if (alias == nullptr || alias->kind != ResolutionKind::ModuleAlias)
                    continue;
                const auto full_path     = alias->modulePath;
                const auto target_module = alias->target.module;
                if (target_module.empty() || full_path.empty())
                    continue;

                auto *target_artifact = [&]() -> const ModuleArtifact * {
                    const auto *member_module = module_by_key.get(target_module);
                    return member_module ? *member_module : nullptr;
                }();
                if (target_artifact == nullptr)
                    continue;

                // Namespace segments like `std.io` are not members of the
                // imported module itself. The outermost field (`println` in
                // `std.io.console.println`) may still match a public symbol;
                // when it is a method instead, the deepest inner field that
                // names an imported type is the symbol sema needs for the
                // receiver (`string` in `string.string.make()`).
                const frontend::Expression &member_node = *chain.front();
                const auto bindSymbol                   = [&](const frontend::Expression &node) {
                    for (const auto &symbol : target_artifact->publicSymbols) {
                        if (symbol.name != node.text)
                            continue;
                        ResolvedName member;
                        member.name            = symbol.name;
                        member.kind            = ResolutionKind::Import;
                        member.span            = node.span;
                        member.target          = {target_module, symbol.id};
                        member.expr            = node.id;
                        member.declKind        = symbol.kind;
                        member.signature       = symbol.signature;
                        member.isExtern        = symbol.isExtern;
                        member.externalSymbol  = symbol.externalSymbol;
                        member.isVariadic      = symbol.isVariadic;
                        member.isVariadicSlice = symbol.isVariadicSlice;
                        member.modulePath      = full_path;
                        resolution.expressions.push_back(std::move(member));
                        return true;
                    }
                    return false;
                };
                bool found = bindSymbol(member_node);
                // `make` in `string.string.make()` is a method on the imported
                // type at the first inner segment, not a public symbol of the
                // module itself. Bind that segment so method resolution can
                // still see the imported receiver type.
                if (!found && chain.size() > 1U) {
                    for (size_t index = chain.size() - 1U; index > 0U; --index) {
                        if (bindSymbol(*chain[index]))
                            break;
                    }
                    found = true;
                }
                if (!found)
                    diagnostics.push_back({diagnostics::Severity::Error, diagnostics::err::NoMember,
                                           "module path '" + joinPath(full_path) +
                                               "' has no public member '" + member_node.text + "'",
                                           module->fileId, member_node.span.start,
                                           member_node.span.end});
                continue;
            }
            if (expression.kind != frontend::ExprKind::Name)
                continue;
            ResolvedName name{
                expression.text, ResolutionKind::Unresolved, expression.span, {}, {}, {},
                expression.scope};
            if (const auto *found = lookupBinding(resolution, expression.text, expression.scope,
                                                  module->frontend->scopes())) {
                name       = *found;
                name.span  = expression.span;
                name.scope = expression.scope;
            }
            name.expr = expression.id;
            resolution.expressions.push_back(std::move(name));
        }
        std::sort(resolution.bindings.begin(), resolution.bindings.end(),
                  [](const ResolvedName &left, const ResolvedName &right) {
                      if (left.name != right.name)
                          return left.name < right.name;
                      return left.scope.value < right.scope.value;
                  });
        // Sorted by node id so a consumer can binary-search by ExprId; spans are
        // ambiguous after macro expansion (every expanded node carries the
        // call-site span).
        std::sort(resolution.expressions.begin(), resolution.expressions.end(),
                  [](const ResolvedName &left, const ResolvedName &right) {
                      if (left.expr.value != right.expr.value)
                          return left.expr.value < right.expr.value;
                      if (left.span.start != right.span.start)
                          return left.span.start < right.span.start;
                      return left.name < right.name;
                  });
        result.push_back(std::move(resolution));
    }
    std::sort(result.begin(), result.end(),
              [](const ModuleResolution &left, const ModuleResolution &right) {
                  return left.module < right.module;
              });
    return result;
}

void FrontendContext::appendCycleDiagnostics(
    const std::vector<ModuleArtifactPtr> &modules,
    const std::map<ModuleKey, std::vector<ModuleKey>> &dependencies,
    const std::vector<ImportEdge> &import_graph, std::vector<ModuleDiagnostic> &diagnostics) {
    memory::FlatMap<ModuleKey, const ModuleArtifact *> by_key;
    memory::FlatMap<ModuleKey, std::vector<ModuleKey>> edges;
    for (const auto &module : modules)
        by_key.insert(module->key, module.get());
    for (const auto &[module, module_dependencies] : dependencies)
        edges.insert(module, module_dependencies);

    enum class Mark : uint8_t { None, Active, Done };
    memory::FlatMap<ModuleKey, Mark> marks;
    std::vector<ModuleKey> stack;
    memory::FlatSet<std::string> reported;
    std::function<void(const ModuleKey &)> visit = [&](const ModuleKey &key) {
        marks.insert(key, Mark::Active);
        stack.push_back(key);
        const auto *module_edges = edges.get(key);
        if (module_edges) {
            for (const auto &next : *module_edges) {
                const auto *mark = marks.get(next);
                if (!mark || *mark == Mark::None) {
                    visit(next);
                    continue;
                }
                if (*mark != Mark::Active)
                    continue;

                const auto begin = std::find(stack.begin(), stack.end(), next);
                std::vector<ModuleKey> cycle(begin, stack.end());
                cycle.push_back(next);
                std::ostringstream message;
                message << "circular import detected: ";
                for (size_t index = 0; index < cycle.size(); ++index) {
                    if (index != 0U)
                        message << " -> ";
                    message << cycle[index];
                }
                if (!reported.insert(message.str()))
                    continue;
                const auto *module = by_key.get(key);
                if (!module)
                    continue;
                frontend::TextSpan span{};
                for (const auto &edge : import_graph) {
                    if (edge.importer == key && std::find(edge.targets.begin(), edge.targets.end(),
                                                          next) != edge.targets.end()) {
                        span = edge.request.span;
                        break;
                    }
                }
                diagnostics.push_back({diagnostics::Severity::Error, diagnostics::err::ImportError,
                                       message.str(), (*module)->fileId, span.start, span.end});
            }
        }
        stack.pop_back();
        marks.insert(key, Mark::Done);
    };
    for (const auto &module : modules) {
        const auto *mark = marks.get(module->key);
        if (!mark || *mark == Mark::None)
            visit(module->key);
    }
}

void FrontendContext::sortDiagnostics(std::vector<ModuleDiagnostic> &diagnostics,
                                      const SourceCatalog &catalog) {
    std::sort(diagnostics.begin(), diagnostics.end(),
              [&catalog](const ModuleDiagnostic &left, const ModuleDiagnostic &right) {
                  const auto left_source  = catalog.find(left.file);
                  const auto right_source = catalog.find(right.file);
                  const auto left_path = left_source ? left_source->canonicalPath : std::string{};
                  const auto right_path =
                      right_source ? right_source->canonicalPath : std::string{};
                  if (left_path != right_path)
                      return left_path < right_path;
                  if (left.start != right.start)
                      return left.start < right.start;
                  if (left.code != right.code)
                      return left.code < right.code;
                  return left.message < right.message;
              });
}

} // namespace zith::session
