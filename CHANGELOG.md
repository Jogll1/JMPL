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
    - `max(x, y)` and `min(x, y)`
    - `floor(x)` and `ceil(x)`
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

## [v0.2.0] - 13/07/2025
### Added
- Finite set literals
    - Sets can be constructed with `{}` and can contain a finite amount of any unique value delimited by commas
- Tuple literal
    - Tuples can be constructed with `()` and can contain a finite amount of any value delimited by commas
    - Tuples can be indexed with `[]` and will return the value at that index
- Set operations
    - `intersect` or `∩` - returns the intersection of two sets
    - `union` or `∪` - returns the union of two sets
    - `\` - returns the relative complement of two sets
    - `subset` or `⊂` - returns true if a set is a proper subset of a set
    - `subseteq` or `⊆` - returns true if a set is a subset of a set
    - `in` or `∈` - returns true if a value is a member of a set
- Cardinality operator `#` - returns the size of any measurable value (sets, tuples, and numbers)
- `for ... in` loop notation to iterate through elements of a set without order
- Omission operation to form sets and tuples 
    - uses format {first, next ... last} where next is optional and all values are integers
### Changed
- `¬=` (not equal operator) to be `/=`
- Max constants in a code chunk changed from 255 to 65535
### Fixed
- Casting numbers to string no longer creates a memory leak
- Indentation bugs

## [v0.2.1] - 17/07/2025
### Added
- Added set-builder notation
- Added existential and universal quantifiers (`∀` or `forall` and `∃` or `exists`)
- `round(x)` native
- Strings can be indexed
- Tuples can be concatenated
- Added `put` keyword that prints output to the console without a newline
### Changed
- `==` (equality operation) to be `=`
### Fixed
- Bugs relating to upvalues and closures
    - Fixed crash when trying to free upvalues
    - Closures popping upvalues from the stack before implicit return
- Invalid infix operators no longer cause a crash
- Optimised HashSet code
- Fixed implicit return