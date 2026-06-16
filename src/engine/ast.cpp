#include "calc/ast.hpp"

#include <utility>
#include <variant>
#include <vector>

namespace calc {

namespace {

// hand a node's direct children to `sink`, leaving the node childless. moving a
// unique_ptr out doesnt destroy anything (it just steals the pointer), so this
// flattens the tree into the worklist without recursing.
void detach_children(Expr &node, std::vector<ExprPtr> &sink) {
  if (auto *u = std::get_if<UnaryOp>(&node.node)) {
    if (u->operand) {
      sink.push_back(std::move(u->operand));
    }
  } else if (auto *b = std::get_if<BinaryOp>(&node.node)) {
    if (b->lhs) {
      sink.push_back(std::move(b->lhs));
    }
    if (b->rhs) {
      sink.push_back(std::move(b->rhs));
    }
  } else if (auto *c = std::get_if<FunctionCall>(&node.node)) {
    for (ExprPtr &arg : c->args) {
      if (arg) {
        sink.push_back(std::move(arg));
      }
    }
  }
}

} // namespace

// free the tree without recursing. a near-cap flat expression (1+1+1+...)
// builds a left-leaning tree ~2000 nodes deep, and the default destructor would
// chase the unique_ptr chain one stack frame per node - fine on an 8mb desktop
// stack, a segfault on the ~256kb-1mb stacks android/qt worker threads run on.
// instead each node drops its children into a worklist shared across the
// teardown, and only the outermost destructor drains it, so the native stack
// never holds more than a single frame no matter how deep the tree is.
Expr::~Expr() {
  static thread_local std::vector<ExprPtr> worklist;
  static thread_local bool draining = false;

  detach_children(*this, worklist);
  if (draining) {
    return; // a nested teardown driven by the drain loop: just unlink and go
  }

  draining = true;
  while (!worklist.empty()) {
    ExprPtr doomed = std::move(worklist.back());
    worklist.pop_back();
    // doomed dies at the end of this iteration -> ~Expr runs with draining set,
    // which unlinks its children into the worklist and returns without
    // recursing. the loop keeps going until the whole tree is gone.
  }
  draining = false;
}

} // namespace calc
