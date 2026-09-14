
/*
 File:    Matt/ASTPattern.h

 Matt's decompiler Abstract Syntax Tree.
 Declarative pattern-combinator engine for the control-flow "pattern finder".

 This replaces the hand-written `do { ...; break; } while(0)` pointer-chasing
 matchers in ASTControlFlow.cc (one per control-flow idiom) with a small
 registry of declarative specs: a spec describes, as data, the sequence of
 typed neighbors a control-flow idiom expects around an anchor node, plus a
 callback that builds the replacement node once a spec matches. See the
 design note at AST.cc:10ff for the motivation (Matt, Nov 21 2025).

 A pattern spec is built with Builder and registered once, e.g. from a file-
 scope static in ASTControlFlowPatterns.cc:

   pattern::Spec BuildLoopPattern() {
     using namespace pattern;
     enum { kBody, kJt };
     return Builder(Tag::Branch, kBwd)
       .Guard([](Node *a){ return a->b() <= a->pc(); })
       .Statements(kBody)
       .Required(Tag::JumpTarget, kJt)
       .JumpPair(Builder::kAnchor, kJt)
       .Name("loop").Priority(10)
       .Build([](Decompiler &dec, Node *anchor, Match &m) -> Node* {
         ...
       });
   }
   bool registerLoop = [] { Register(BuildLoopPattern()); return true; }();

 Written by:  Matt, 2025.
 */

#if !defined(__MATT_AST_PATTERN_H)
#define __MATT_AST_PATTERN_H 1

#include "Matt/AST.h"
#include "Matt/ASTControlFlowHelper.h"  // for JumpTarget, used by JumpPair()

#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>

class Decompiler;

namespace ast {
namespace pattern {

/** Walks prev/next from a starting node in a fixed direction. */
class Cursor {
  Node *pos_;
  Direction dir_;
public:
  Cursor(Node *start, Direction dir) : pos_(start), dir_(dir) { }
  /** The node at the cursor, or nullptr past the root list's First/LastNode. */
  Node *peek() const { return (pos_ && pos_->provides() != kSpecialNode) ? pos_ : nullptr; }
  void advance() { pos_ = (dir_ == kFwd) ? pos_->next : pos_->prev; }
  Direction direction() const { return dir_; }
};

/**
 \brief The result of a (partial or complete) pattern match: nodes and node
 runs captured into small integer slot ids that the spec's own enum defines.
 */
class Match {
  Node *anchor_;
  std::map<int, Node*> nodes_;
  std::map<int, bool> present_;
  std::map<int, std::vector<Node*>> runs_;
  std::map<int, std::vector<Match>> repeats_;
public:
  explicit Match(Node *anchor) : anchor_(anchor) { }
  Node *anchor() const { return anchor_; }

  void SetNode(int slot, Node *nd) { nodes_[slot] = nd; present_[slot] = true; }
  void SetAbsent(int slot) { present_[slot] = false; }
  void SetRun(int slot, std::vector<Node*> run) { runs_[slot] = std::move(run); }
  void SetRepeats(int slot, std::vector<Match> reps) { repeats_[slot] = std::move(reps); }

  Node *node(int slot) const {
    auto it = nodes_.find(slot);
    return (it == nodes_.end()) ? nullptr : it->second;
  }
  template <class T> T *as(int slot) const { return dynamic_cast<T*>(node(slot)); }
  /** Whether an Optional() slot actually matched a node. */
  bool has(int slot) const {
    auto it = present_.find(slot);
    return (it != present_.end()) && it->second;
  }
  const std::vector<Node*> &run(int slot) const {
    static const std::vector<Node*> kEmpty;
    auto it = runs_.find(slot);
    return (it == runs_.end()) ? kEmpty : it->second;
  }
  const std::vector<Match> &repeats(int slot) const {
    static const std::vector<Match> kEmpty;
    auto it = repeats_.find(slot);
    return (it == repeats_.end()) ? kEmpty : it->second;
  }
};

using GuardFn = std::function<bool(Node *anchor)>;
using Cond = std::function<bool(Node *candidate)>;
using CountFn = std::function<int(Node *anchor)>;
using Callback = std::function<Node*(Decompiler &dec, Node *anchor, Match &m)>;

/** One step of a spec: try to advance the cursor and/or populate the match;
    return false only if the WHOLE pattern should be rejected. */
using Step = std::function<bool(Cursor&, Match&)>;

struct Spec {
  Tag anchorTag = Tag::Any;
  Direction dir = kFwd;
  GuardFn guard;
  std::vector<Step> steps;
  Callback callback;
  int priority = 100;
  std::string name;
};

/**
 \brief Declarative builder for one control-flow pattern spec.
 Also used, sans anchor/direction, as a sub-pattern inside Repeat().
 */
class Builder {
  Tag anchorTag_ = Tag::Any;
  Direction dir_ = kFwd;
  GuardFn guard_;
  std::vector<Step> steps_;
  int priority_ = 100;
  std::string name_;

public:
  /// Sentinel slot id meaning "the pattern's anchor node itself", usable
  /// wherever a captured slot id is expected (currently only JumpPair()).
  static constexpr int kAnchor = -1;

  Builder() = default;
  Builder(Tag anchorTag, Direction dir) : anchorTag_(anchorTag), dir_(dir) { }

  Builder &Guard(GuardFn g) { guard_ = std::move(g); return *this; }

  Builder &Required(Tag t, int slot, bool mustBeResolved = false) {
    return Required(t, slot, Cond(), mustBeResolved);
  }
  Builder &Required(Tag t, int slot, Cond cond, bool mustBeResolved = false) {
    steps_.push_back([t, slot, cond, mustBeResolved](Cursor &c, Match &m) -> bool {
      Node *nd = c.peek();
      if (!nd) return false;
      if (mustBeResolved && !nd->Resolved()) return false;
      if (t != Tag::Any && nd->tag() != t) return false;
      if (cond && !cond(nd)) return false;
      m.SetNode(slot, nd);
      c.advance();
      return true;
    });
    return *this;
  }

  Builder &Optional(Tag t, int slot, bool mustBeResolved = false) {
    return Optional(t, slot, Cond(), mustBeResolved);
  }
  Builder &Optional(Tag t, int slot, Cond cond, bool mustBeResolved = false) {
    steps_.push_back([t, slot, cond, mustBeResolved](Cursor &c, Match &m) -> bool {
      Node *nd = c.peek();
      bool ok = nd
        && (!mustBeResolved || nd->Resolved())
        && (t == Tag::Any || nd->tag() == t)
        && (!cond || cond(nd));
      if (ok) { m.SetNode(slot, nd); c.advance(); }
      else { m.SetAbsent(slot); }
      return true;
    });
    return *this;
  }

  /** Capture a variable-length (possibly empty) run of consecutive statement
      nodes directly off the flat list -- no physical CodeBlock required. */
  Builder &Statements(int slot) {
    steps_.push_back([slot](Cursor &c, Match &m) -> bool {
      std::vector<Node*> run;
      while (Node *nd = c.peek()) {
        if (!nd->IsStatement()) break;
        run.push_back(nd);
        c.advance();
      }
      if (c.direction() == kBwd) std::reverse(run.begin(), run.end());
      m.SetRun(slot, std::move(run));
      return true;
    });
    return *this;
  }

  /** Reject the whole match unless a prior Statements(slot) captured at
      least one node. Some idioms (e.g. `if...then`) never omit their body
      entirely the way `loop`/`while`/`repeat` can (which default to a `nil`
      body via NewNil() instead) -- this makes that requirement explicit
      rather than silently accepting an empty run where the original
      hand-written matcher would have `break`d out. */
  Builder &NonEmpty(int slot) {
    steps_.push_back([slot](Cursor&, Match &m) -> bool {
      return !m.run(slot).empty();
    });
    return *this;
  }

  /** Match `sub` `count(anchor)` times in a row, capturing one sub-Match per
      iteration. `sub` is direction-less; it walks the same cursor as the
      enclosing spec. */
  Builder &Repeat(int slot, CountFn count, Builder sub) {
    steps_.push_back([slot, count, sub](Cursor &c, Match &m) -> bool {
      int n = count(m.anchor());
      std::vector<Match> reps;
      reps.reserve(n > 0 ? n : 0);
      for (int i = 0; i < n; i++) {
        Match subMatch(m.anchor());
        if (!sub.RunSteps(c, subMatch)) return false;
        reps.push_back(std::move(subMatch));
      }
      m.SetRepeats(slot, std::move(reps));
      return true;
    });
    return *this;
  }

  /** Require that the JumpTarget captured at `targetSlot` is a consistent
      origin/destination pair with the node at `originSlot` (or kAnchor for
      the pattern's own anchor node). Does not advance the cursor. */
  Builder &JumpPair(int originSlot, int targetSlot) {
    steps_.push_back([originSlot, targetSlot](Cursor&, Match &m) -> bool {
      Node *origin = (originSlot == kAnchor) ? m.anchor() : m.node(originSlot);
      Node *targetNode = (targetSlot == kAnchor) ? m.anchor() : m.node(targetSlot);
      auto *jt = dynamic_cast<JumpTarget*>(targetNode);
      if (!origin || !jt) return false;
      return JumpPairMatches(origin, jt);
    });
    return *this;
  }

  Builder &Name(const char *n) { name_ = n; return *this; }
  Builder &Priority(int p) { priority_ = p; return *this; }

  /** Used internally by Repeat() to run a sub-Builder's steps against the
      shared cursor of the enclosing spec. */
  bool RunSteps(Cursor &c, Match &m) const {
    for (auto &step : steps_) {
      if (!step(c, m)) return false;
    }
    return true;
  }

  Spec Build(Callback cb) {
    Spec spec;
    spec.anchorTag = anchorTag_;
    spec.dir = dir_;
    spec.guard = guard_;
    spec.steps = steps_;
    spec.callback = std::move(cb);
    spec.priority = priority_;
    spec.name = name_;
    return spec;
  }
};

/** Register a completed pattern spec. Call once, e.g. from a file-scope
    static initializer in the .cc file that defines the idiom. */
void Register(Spec spec);

/**
 \brief Try every pattern spec registered for `anchor`'s tag(), in priority
 order (lowest first), returning the replacement node from the first spec
 whose steps all succeed and whose callback returns non-null.
 \return the replacement node, or nullptr if no registered pattern matched.
 */
Node *TryResolve(Node *anchor);

} // namespace pattern
} // namespace ast

#endif  /* __MATT_AST_PATTERN_H */
