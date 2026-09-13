
/*
 File:    Matt/ASTPattern.cc

 Matt's decompiler Abstract Syntax Tree.
 Declarative pattern-combinator engine for the control-flow "pattern finder".
 See Matt/ASTPattern.h for the design rationale.

 Written by:  Matt, 2025.
 */

#include "Matt/ASTPattern.h"
#include "Matt/Decompiler.h"

#include <algorithm>

namespace ast {
namespace pattern {

namespace {
std::multimap<Tag, Spec> &Registry() {
  static std::multimap<Tag, Spec> registry;
  return registry;
}
} // namespace

void Register(Spec spec) {
  Tag key = spec.anchorTag;
  Registry().emplace(key, std::move(spec));
}

Node *TryResolve(Node *anchor) {
  auto &registry = Registry();
  auto range = registry.equal_range(anchor->tag());
  if (range.first == range.second) return nullptr;

  std::vector<const Spec*> candidates;
  for (auto it = range.first; it != range.second; ++it) {
    candidates.push_back(&it->second);
  }
  std::stable_sort(candidates.begin(), candidates.end(),
                    [](const Spec *a, const Spec *b) { return a->priority < b->priority; });

  for (const Spec *spec : candidates) {
    if (spec->guard && !spec->guard(anchor)) continue;

    Match m(anchor);
    Node *start = (spec->dir == kFwd) ? anchor->next : anchor->prev;
    Cursor cursor(start, spec->dir);

    bool matched = true;
    for (const Step &step : spec->steps) {
      if (!step(cursor, m)) { matched = false; break; }
    }
    if (!matched) continue;

    Node *result = spec->callback(anchor->Dec(), anchor, m);
    if (result) return result;
  }
  return nullptr;
}

} // namespace pattern
} // namespace ast
