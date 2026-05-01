# Contributing to noodles

We want to make contributing to this project as easy and transparent as
possible.

## Our Development Process

`noodles` is developed primarily inside Meta's monorepo and synced to GitHub
in a one-way mirror. Internal changes land first and then appear here as
batched commits. We do accept pull requests through GitHub — they are
imported into the monorepo, reviewed there, and pushed back out as part of
the next sync.

## Pull Requests

We actively welcome your pull requests.

1. Fork the repo and create your branch from `main`.
2. If you've added code that should be tested, add tests.
3. If you've changed APIs, update the documentation.
4. Ensure the test suite passes (`ctest --test-dir build`).
5. Make sure your code builds cleanly with the supported compilers
   (recent GCC, Clang, and MSVC) on Linux, macOS, and Windows.
6. If you haven't already, complete the Contributor License Agreement
   ("CLA").

## Contributor License Agreement ("CLA")

In order to accept your pull request, we need you to submit a CLA. You only
need to do this once to work on any of Meta's open source projects.

Complete your CLA here: <https://code.facebook.com/cla>

## Issues

We use GitHub issues to track public bugs. Please ensure your description is
clear and has sufficient instructions to be able to reproduce the issue.

Meta has a [bounty program](https://bugbounty.meta.com/) for the safe
disclosure of security bugs. In those cases, please go through the process
outlined on that page and do not file a public issue.

## Coding Style

* Follow the existing C++17 style in the repo: 2-space indentation,
  `PascalCase` for types, `camelCase` for functions and variables, snake_case
  only for filesystem-style paths.
* Keep public headers minimal — implementation details belong in `.cpp`
  files.
* Use `const` and `constexpr` aggressively.
* Prefer references over pointers; prefer `std::unique_ptr` over raw owning
  pointers.

## License

By contributing to `noodles`, you agree that your contributions will be
licensed under the LICENSE.txt file in the root directory of this source
tree.
