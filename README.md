# ox.stacktrace library

## Description

Ox.stacktrace is a `header-only` library
This is a library that supports to get stack traces in C++.

You can use it on windows, almost all unix-like systems, etc.

## Usage

```cpp

#include <ox/stacktrace.hpp>

using namespace ox;

int main() {
    stacktrace::print(); // print current stack trace

    return 0;
}
```

Note:
- ox.stacktrace needs symbols for getting symbols.
- so you need to use
-   `xmake f -m releasedbg` in xmake or
-   `cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo` in cmake or
-   `/Zi` in cl.exe or
-   `-rdynamic` in gcc
