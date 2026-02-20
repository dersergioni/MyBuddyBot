# Contributing to MyBuddyBot

Thank you for your interest in contributing to MyBuddyBot!

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/dersergioni/MyBuddyBot.git
   cd MyBuddyBot
   ```
3. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

See the [README.md](README.md) for detailed build instructions.

Quick start:
```bash
# Install dependencies (macOS)
brew install cmake ninja boost openssl curl fmt rapidjson sqlite3 googletest

# Build
cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja -B build
cmake --build build

# Run tests
cd build && ctest --output-on-failure
```

## Code Style

- **Language:** C++20
- **Formatting:** Use the `.clang-format` file in the repository root
  ```bash
  clang-format -i <file>
  ```
- **Naming conventions:**
  - Classes/Methods: `PascalCase`
  - Variables: `camelCase`
  - Private members: `camelCase_` (trailing underscore)
  - Constants: `kPascalCase`

## Making Changes

1. **Write tests** for new functionality
2. **Ensure all tests pass** before submitting
3. **Keep commits focused** - one logical change per commit
4. **Write clear commit messages:**
   ```
   Add support for new AI provider

   - Implement XyzService class
   - Add configuration options
   - Update documentation
   ```

## Pull Request Process

1. **Update documentation** if needed
2. **Ensure CI passes** (build and tests)
3. **Fill out the PR template** with:
   - Description of changes
   - Related issues
   - Testing performed
4. **Request review** from maintainers

## Reporting Issues

- Use GitHub Issues for bug reports and feature requests
- Include reproduction steps for bugs
- Check existing issues before creating new ones

## Questions?

Feel free to open a discussion or reach out to the maintainers.
