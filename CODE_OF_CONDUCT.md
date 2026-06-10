# Code of Conduct

## Project Standards

Strictly compliant C23 and follows Linux kernel coding style with these prohibitions:
- No Variable Length Arrays (VLA)
- No implicit int declarations
- No compiler extensions
- No non-standard C features

### Compiler Configuration

Required Flags:

- `-std=c23`: Strict C23 standard
- `-pedantic`: Reject non-ISO C
- `-Wall -Wextra`: Enable all warnings
- `-Wvla`: Warn about VLA usage
- `-Wimplicit-int`: Warn about implicit int
- `-Werror=implicit-function-declaration`: Error on implicit declarations

## Coding Style

This project follows [Linux Kernel Coding](https://www.kernel.org/doc/html/latest/process/coding-style.html) conventions:

### Brace Style (K&R variant)
- Function opening braces on new line
- Control statement braces on same line
- Always use braces for multi-line blocks

### Indentation and Spacing
- Use tabs for indentation (8 spaces per tab)
- No trailing whitespace
- Maximum line length: 80 characters
- Space after keywords: `if (`, `for (`, `while (`
- No space after function names: `function()`

### Naming Conventions
- `snake_case` for variables and functions
- `UPPER_SNAKE_CASE` for macros and constants
- Descriptive names that indicate purpose

### Type Usage
- Use `size_t` for sizes and indices
- Use standard integer types: `int8_t`, `uint32_t`, etc.
- Avoid `bool` from stdbool.h (use `int` with 0/1)

### Header Files
- Include guards with `#ifndef HEADER_H` / `#define HEADER_H`
- Minimal includes, only what's necessary
- Function prototypes for all public functions
- [Doxygen-style comments](https://www.doxygen.nl/manual/index.html) for public APIs
