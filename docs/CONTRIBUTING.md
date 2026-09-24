// Content directly from github.com/rexglue/rexglue-sdk

Guidelines for contributing to the EdgeOfTimeRecomp, covering build setup, code style, and the pull request workflow.

## Prerequisites

- **CMake** 3.25+
- **Clang** 20+
- **Ninja**

See [[Getting Started]] for full environment setup instructions.

## Building

See the [BUILDING.md](BUILDING.md) guide for build setup and platform-specific instructions.

## Code Style

The project uses [**Google C++ Style**](https://google.github.io/styleguide/cppguide.html) with project-specific conventions.

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / Structs | PascalCase | `ReeotApp` |
| Functions / Methods | PascalCase | `OnFinalizePaths()` |
| Local variables | snake_case | `xex_image` |
| Member variables | snake_case + trailing `_` | `install_root_` |
| Constants | k + PascalCase | `kExecutable` |
| Enum values | k + PascalCase | `kRequiresRestart` |
| Namespaces | lowercase, `::` nested | `eot::installer` |
| Macros | UPPER_CASE | `EOT_NOOP` |

### Formatting Rules

- **Line endings:** LF (`\n`) only -- no CRLF. Configure your editor and Git accordingly (see [Git Workflow](#git-workflow) below).
- **Indentation:** 2 spaces (no tabs)
- **Column limit:** 100
- **Braces:** K&R (opening brace on same line)
- **Namespace contents:** Not indented
- **Namespace closing:** `} // namespace eot::foo`
- **Pointers / References:** Left-aligned (`int* p`, `int& r`)
- **Const placement:** West const (`const int x`)
- **Access specifiers:** 1-space indent (` public:`)
- **Include guards:** `#pragma once`
- **Project includes:** Angle brackets (`<gamelogic/...>`)
- **Include order:** C headers, C++ STL, platform headers, third-party, `<goliath/...>`
- **Preferred:** `using` over `typedef`

## Git Workflow

### Line Endings

This project uses **LF line endings exclusively**. Windows developers must configure Git to handle this correctly:

```bash
git config core.autocrlf input
```

With `core.autocrlf=input`, Git will convert CRLF to LF on commit but leave LF untouched on checkout.

If your editor is inserting CRLF, configure it to use LF:
- **VS Code:** Set `"files.eol": "\n"` in settings
- **CLion:** Settings > Editor > Code Style > Line separator: "Unix and macOS (\\n)"

### Linear History

This project uses a **linear history** -- no merge commits. Always rebase your branch onto `upstream/main` (or similar) before creating a pull request.

> [!IMPORTANT]
> If you have conflicts, resolve them during the rebase.
> Never use `git merge` to update your branch.

### Reversing Notes

Contributors working on game functionality must reverse the relevant game code and data before implementing changes. Each change should explain why the chosen approach is appropriate for the game, including the evidence found in the original code, data structures, call sites, or runtime behavior. Avoid making assumptions about undocumented behavior: when adding or changing XYZ, document what it does in the game and why the implementation matches that behavior.

If you need help with reversing notes or interpreting the game's behavior, contact us on [Discord](https://discord.gg/PsReBEDDZX). Maff, Serjar, and I have practically reversed the entire game and can help provide context for areas that have already been investigated.


### Submitting Changes

1. Create a branch from your forked repo's `main`
2. Make your changes
3. Rebase onto the latest `upstream/main` (or similar) before pushing
4. Push and open a pull request against `main`

> [!IMPORTANT]
> All pull requests must have an associated issue (or issues). Link them in the PR description. If no issue exists for what you're working on, create one first.
