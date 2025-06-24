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
  - `Max()` and `min()`
- Escape characters can now be printed to the console
### Changed
- Statements no longer need to end with a semicolon (';')
### Fixed
- The trigonometric functions
  - `sin(n * pi())` will now return 0 for any n
  - `tan(n * pi())` will now return 0 for any n
  - `cos((2 * n - 1) * pi())` will now return 0 for any odd n
  - `tan(n * pi() / 2)` will now return null for any n