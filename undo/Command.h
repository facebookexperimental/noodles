// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_UNDO_COMMAND_H
#define NOODLES_UNDO_COMMAND_H

#include "core/api.h"

#include <memory>
#include <string>

namespace noodles {

/// Abstract base class for undoable commands.
///
/// Each command captures the state needed to both apply and reverse an edit.
/// Commands are stored on the undo/redo stacks in NoodlesUndoManager.
///
/// - execute() applies (or re-applies) the edit
/// - undo() reverses the edit
/// - Both must be safe to call multiple times (for redo/undo cycles)
class NOODLES_API Command {
 public:
  virtual ~Command() = default;

  /// Apply the edit. Also used for redo.
  virtual void execute() = 0;

  /// Reverse the edit.
  virtual void undo() = 0;

  /// Human-readable description (e.g. "Move Nodes", "Delete Connection").
  virtual std::string description() const = 0;
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace noodles

#endif // NOODLES_UNDO_COMMAND_H
