// Copyright (c) Meta Platforms, Inc. and affiliates.
//
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef NOODLES_UNDO_LAMBDA_COMMAND_H
#define NOODLES_UNDO_LAMBDA_COMMAND_H

#include "undo/Command.h"

#include <functional>
#include <string>

namespace noodles {

/// A command that wraps two callable objects for execute and undo.
/// Primary bridge for Python usage via boost::python callables.
class NOODLES_API LambdaCommand : public Command {
 public:
  LambdaCommand(
      std::string description,
      std::function<void()> doFunc,
      std::function<void()> undoFunc);
  ~LambdaCommand() override = default;

  // Move-only to be safe with std::function captures.
  LambdaCommand(const LambdaCommand&) = delete;
  LambdaCommand& operator=(const LambdaCommand&) = delete;
  LambdaCommand(LambdaCommand&&) = default;
  LambdaCommand& operator=(LambdaCommand&&) = default;

  void execute() override;
  void undo() override;
  std::string description() const override;

 private:
  std::string _description;
  std::function<void()> _doFunc;
  std::function<void()> _undoFunc;
};

} // namespace noodles

#endif // NOODLES_UNDO_LAMBDA_COMMAND_H
