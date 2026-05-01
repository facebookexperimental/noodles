// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_UNDO_COMPOUND_COMMAND_H
#define NOODLES_UNDO_COMPOUND_COMMAND_H

#include "undo/Command.h"

#include <string>
#include <vector>

namespace noodles {

/// Groups multiple commands into a single undoable operation.
/// execute() runs children in insertion order.
/// undo() runs children in reverse order.
class NOODLES_API CompoundCommand : public Command {
 public:
  explicit CompoundCommand(std::string description);
  ~CompoundCommand() override = default;

  // Move-only: vector<unique_ptr> is not copyable.
  CompoundCommand(const CompoundCommand&) = delete;
  CompoundCommand& operator=(const CompoundCommand&) = delete;
  CompoundCommand(CompoundCommand&&) = default;
  CompoundCommand& operator=(CompoundCommand&&) = default;

  /// Add a child command. Ownership is transferred.
  void addCommand(CommandPtr cmd);

  void execute() override;
  void undo() override;
  std::string description() const override;

  bool empty() const;
  size_t size() const;

 private:
  std::vector<CommandPtr> _commands;
  std::string _description;
};

} // namespace noodles

#endif // NOODLES_UNDO_COMPOUND_COMMAND_H
