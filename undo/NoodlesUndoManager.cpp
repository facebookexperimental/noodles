// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "NoodlesUndoManager.h"

#include <cassert>
#include <thread>

namespace noodles {

#ifndef NDEBUG
namespace {
// Captures the thread ID on first mutating call; asserts same thread thereafter.
std::thread::id g_ownerThread{};
void assertMainThread() {
  auto current = std::this_thread::get_id();
  if (g_ownerThread == std::thread::id{}) {
    g_ownerThread = current;
  }
  assert(current == g_ownerThread && "NoodlesUndoManager must be called from the main/UI thread");
}
} // namespace
#else
namespace {
void assertMainThread() {}
} // namespace
#endif

NoodlesUndoManager& NoodlesUndoManager::instance() {
  static NoodlesUndoManager undoManager;
  return undoManager;
}

void NoodlesUndoManager::pushCommand(CommandPtr cmd) {
  assertMainThread();
  if (!cmd) {
    return;
  }
  _redoStack.clear();
  _undoStack.push_back(std::move(cmd));

  if (_undoStack.size() > _maxStackDepth) {
    _undoStack.erase(
        _undoStack.begin(),
        _undoStack.begin() + static_cast<ptrdiff_t>(_undoStack.size() - _maxStackDepth));
  }
}

void NoodlesUndoManager::undo() {
  assertMainThread();
  if (_undoStack.empty()) {
    return;
  }

  // Move to redo stack *before* calling undo() so the command isn't lost
  // if the callback throws (e.g. a Python exception).
  _redoStack.push_back(std::move(_undoStack.back()));
  _undoStack.pop_back();

  _redoStack.back()->undo();
}

void NoodlesUndoManager::redo() {
  assertMainThread();
  if (_redoStack.empty()) {
    return;
  }

  // Move to undo stack *before* calling execute() so the command isn't lost
  // if the callback throws (e.g. a Python exception).
  _undoStack.push_back(std::move(_redoStack.back()));
  _redoStack.pop_back();

  _undoStack.back()->execute();
}

bool NoodlesUndoManager::canUndo() const {
  return !_undoStack.empty();
}

bool NoodlesUndoManager::canRedo() const {
  return !_redoStack.empty();
}

void NoodlesUndoManager::clear() {
  assertMainThread();
  _undoStack.clear();
  _redoStack.clear();
}

std::string NoodlesUndoManager::undoDescription() const {
  if (_undoStack.empty()) {
    return {};
  }
  return _undoStack.back()->description();
}

std::string NoodlesUndoManager::redoDescription() const {
  if (_redoStack.empty()) {
    return {};
  }
  return _redoStack.back()->description();
}

void NoodlesUndoManager::setMaxStackDepth(size_t depth) {
  _maxStackDepth = depth;
  if (_undoStack.size() > _maxStackDepth) {
    _undoStack.erase(
        _undoStack.begin(),
        _undoStack.begin() + static_cast<ptrdiff_t>(_undoStack.size() - _maxStackDepth));
  }
  if (_redoStack.size() > _maxStackDepth) {
    _redoStack.erase(
        _redoStack.begin(),
        _redoStack.begin() + static_cast<ptrdiff_t>(_redoStack.size() - _maxStackDepth));
  }
}

} // namespace noodles
