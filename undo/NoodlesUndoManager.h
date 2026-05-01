// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_UNDO_UNDO_MANAGER_H
#define NOODLES_UNDO_UNDO_MANAGER_H

#include "core/api.h"
#include "undo/Command.h"

#include <string>
#include <vector>

namespace noodles {

/// Global singleton undo manager that owns the full undo/redo stack.
///
/// Stores Command objects that know how to execute() and undo() themselves.
/// The manager simply moves commands between the undo and redo stacks.
///
/// Thread-safety: All methods must be called from the main/UI thread.
class NOODLES_API NoodlesUndoManager {
 public:
  static NoodlesUndoManager& instance();

  NoodlesUndoManager(const NoodlesUndoManager&) = delete;
  NoodlesUndoManager& operator=(const NoodlesUndoManager&) = delete;
  NoodlesUndoManager(NoodlesUndoManager&&) = delete;
  NoodlesUndoManager& operator=(NoodlesUndoManager&&) = delete;

  /// Pushes a command onto the undo stack.
  /// Clears redo stack (new edit after undo invalidates redo history).
  void pushCommand(CommandPtr cmd);

  /// Pops from undo stack, calls cmd->undo(), pushes to redo stack.
  void undo();

  /// Pops from redo stack, calls cmd->execute(), pushes to undo stack.
  void redo();

  bool canUndo() const;
  bool canRedo() const;

  /// Empties both stacks (call on graph load).
  void clear();

  std::string undoDescription() const;
  std::string redoDescription() const;

  void setMaxStackDepth(size_t depth);

 private:
  NoodlesUndoManager() = default;
  ~NoodlesUndoManager() = default;

  static constexpr size_t kDefaultMaxStackDepth = 100;

  std::vector<CommandPtr> _undoStack;
  std::vector<CommandPtr> _redoStack;
  size_t _maxStackDepth = kDefaultMaxStackDepth;
};

} // namespace noodles

#endif // NOODLES_UNDO_UNDO_MANAGER_H
