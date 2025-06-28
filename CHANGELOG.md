# Changelog

## [v0.1.0] - 13/12/2024
### Added
- An interpreter built in C (c_jmpl)
- The basic syntax of the language
  - Comments
  - Output
  - Variables
  - Basic operators (+, -, *, /, %, ^, comparison, etc.)
  - If and While statements
  - Functions with implicit and explicit return
- Basic native functions
  - `clock()`
  - Trigonometry functions

## [v0.1.1] - 24/06/2025
### Added
- New native functions
  - `pi()`
  - `max()` and `min()`
  - `floor()` and `ceil()`
### Changed
- Statements no longer need to end with a semicolon (`;`)
- Blocks use indents instead of curly braces (`{}`)
### Fixed
- The trigonometric functions
  - `sin(n * pi())` will now return 0 for any integer n
  - `tan(n * pi())` will now return 0 for any integer n
  - `cos((2 * n - 1) * pi())` will now return 0 for any odd integer n
  - `tan(n * pi() / 2)` will now return null for any integer n
- Escape characters can now be properly printed to the console