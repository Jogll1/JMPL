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
- Range operation to form sets and tuples 
    - uses format {first, next ... last} where next is optional and all values are integers
### Changed
- `¬=` (not equal operator) to be `/=`
- Max constants in a code chunk changed from 255 to 65535
### Fixed
- Casting numbers to string no longer creates a memory leak
- Indentation bugs

## [v0.2.1] - 3/08/2025
### Added
- Added set-builder notation
- Added quantifiers (`∀` or `forall`, `∃` or `exists`, and `some`)
- New native functions
    - `round(x)`
    - `print(x)` and `println(x)`
    - `sleep(x)`
- Strings can be indexed
- Tuples can be concatenated
- A 1-tuple can be created using `(x,)`
### Changed
- Changed `==` (equality operation) to be `=`
- Removed `out` keyword and replaced it with the functions `print(x)` and `println(x)`
### Fixed
- Bugs relating to upvalues and closures
    - Fixed crash when trying to free upvalues
    - Closures popping upvalues from the stack before implicit return
- Invalid infix operators no longer cause a crash
- Optimised HashSet code
- Fixed implicit return
- Fixed garbage collector collecting sets while they are being created
- Optimised Value code with NaN boxing

## [v0.2.2] - 01/09/2025
### Added
- New native functions to core
    - `type(x)`
    - `input()`
    - `num(x)`
    - `str(x)`
    - `char(x)`
- Character data type, created with single quotes (`''`)
- Several new escape sequence types (with 'H' meaning a hex character): `\uHHHH`, `\UHHHHHH`, `\xHH`, `\a`, `\b`, `\v`, `\f`, `\0`
- Strings and tuple can now be sliced using `[x ... y]` subscript notation
- Anonymous function expressions
- Module system where modules can be imported using `with`
- Added a module called `"random"` that provides functions with PRNG capabilities
### Changed
- Any syntax that uses generators (for loops, set builders, quantifiers) can now use any iterable object instead of just sets.
- Quantfiers now use a pipe (`|`) instead of a comma (`,`) to be consistent with set-builder and for-loop syntax
- Reversed `=` (equality operation) to be `==`
- Indexing a string now returns a character
- Made the `arb` keyword return a random member of a set
- Optimisations for strings and sets
- Strings and tuples can now be indexed with negative indices to get the last elements (this extends to slicing as well)
- Moved math native functions to their own module called `"math"`
- Characters can now be compared using >, >=, <, and, <= 
- Made characters work with range operations
### Fixed
- Fixed multiple garabage collection errors
- Strings have been refactored to store character Unicode code points as well as UTF-8 bytes
    - Now Unicode strings can be indexed and sized correctly
- Tuples concatenating no longer causes a memory leak
- Compiler can now report multiple errors meaning the REPL no longer finishes when encountering a syntax error
- Invalid ranges now create an empty set or tuple
- Comments at the start of a control flow block no longer causes an error