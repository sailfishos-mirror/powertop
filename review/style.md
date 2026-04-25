# PowerTOP Coding Style Guide

This document outlines the coding style and conventions used in the PowerTOP project, deduced from the existing codebase in the `src/` directory.

## 1. Indentation and Formatting

### 1.1 Indentation
*   **Tabs only**: Use tabs for indentation.
*   **Tab Width**: Tabs should be treated as 8 spaces wide.
*   **No trailing whitespace**: Lines should not end with whitespace.

### 1.2 Braces and Control Flow
*   **K&R Style (Linux Kernel Variant)**:
    *   For `if`, `while`, `for`, `switch`, the opening brace `{` is on the same line as the statement.
    *   For function definitions, the opening brace `{` is on a new line.
    *   The closing brace `}` is on its own line, except for `else` which is placed between braces: `} else {`.

```cpp
void function_name()
{
	if (condition) {
		// code
	} else {
		// code
	}

	while (condition) {
		// code
	}
}
```

### 1.3 Switch Statements
*   Align `case` labels with the `switch` statement.
*   Indent the code within each `case`.

```cpp
switch (value) {
case 1:
	do_something();
	break;
default:
	break;
}
```

## 2. Naming Conventions

### 2.1 Files
*   Use lowercase and snake_case for filenames (e.g., `devlist.cpp`, `report-maker.cpp`).
*   C++ source files use `.cpp` extension.
*   Header files use `.h` extension.

### 2.2 Variables and Functions
*   **Variables**: Use `snake_case` (e.g., `debug_learning`, `time_out`).
*   **Functions**: Use `snake_case` (e.g., `print_usage`, `do_sleep`).

### 2.3 Classes and Structs
*   **Classes**: Use `snake_case` (e.g., `device`, `abstract_cpu`).
*   **Structs**: Use `snake_case` (e.g., `idle_state`, `frequency`).
*   **Methods**: Use `snake_case` (e.g., `start_measurement`, `human_name`).

### 2.4 Macros and Constants
*   Use `UPPER_SNAKE_CASE` (e.g., `DEBUGFS_MAGIC`, `NR_OPEN_DEF`, `OPT_AUTO_TUNE`).

## 3. Comments

### 3.1 File Header
*   Every file must start with the standard Copyright and GPL license header.

```cpp
/*
 * Copyright 2010, Intel Corporation
 *
 * This file is part of PowerTOP
 * ... (GPL License text)
 * Authors:
 *	Name <email>
 */
```

### 3.2 Block and Inline Comments
*   Use C-style `/* ... */` for block comments and multi-line explanations.
*   C++ style `//` is acceptable for short, single-line comments.

## 4. C++ Practices

### 4.1 Header Guards
*   Use `#pragma once`

### 4.2 Namespaces
*   The project frequently uses `using namespace std;` in both source and header files. While generally discouraged in headers, this is the established pattern in the codebase.

### 4.3 Includes
*   Order: Standard library headers first, followed by project-specific headers.
*   Project headers should be included using quotes (e.g., `#include "lib.h"`).

## 5. Internationalization (i18n)

*   All user-facing strings must be wrapped in the gettext macro `_("string")` or `__("string")` for internationalization support.

## 6. Error Handling

*   Use `fprintf(stderr, ...)` for error reporting.
*   Fatal errors that cannot be recovered from should use `abort()` or `exit(EXIT_FAILURE)` after notifying the user.
