// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include "LambdaCommand.h"

namespace noodles {

LambdaCommand::LambdaCommand(
    std::string description,
    std::function<void()> doFunc,
    std::function<void()> undoFunc)
    : _description(std::move(description)),
      _doFunc(std::move(doFunc)),
      _undoFunc(std::move(undoFunc)) {}

void LambdaCommand::execute() {
  if (_doFunc) {
    _doFunc();
  }
}

void LambdaCommand::undo() {
  if (_undoFunc) {
    _undoFunc();
  }
}

std::string LambdaCommand::description() const {
  return _description;
}

} // namespace noodles
