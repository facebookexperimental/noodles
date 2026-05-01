// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "CompoundCommand.h"

namespace noodles {

CompoundCommand::CompoundCommand(std::string description) : _description(std::move(description)) {}

void CompoundCommand::addCommand(CommandPtr cmd) {
  if (cmd) {
    _commands.push_back(std::move(cmd));
  }
}

void CompoundCommand::execute() {
  for (auto& cmd : _commands) {
    cmd->execute();
  }
}

void CompoundCommand::undo() {
  // NOLINTNEXTLINE(modernize-loop-convert) — reverse range-for doesn't compile under platform010
  // clang15/libstdc++11
  for (auto it = _commands.rbegin(); it != _commands.rend(); ++it) {
    (*it)->undo();
  }
}

std::string CompoundCommand::description() const {
  return _description;
}

bool CompoundCommand::empty() const {
  return _commands.empty();
}

size_t CompoundCommand::size() const {
  return _commands.size();
}

} // namespace noodles
