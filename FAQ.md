# S Lua - Frequently Asked Questions

## General Questions

### Q: What is S Lua?
**A:** S Lua is a statically-typed, compiled systems programming language designed for performance-critical applications, game development, and systems work. It features static typing, memory control, and modern language constructs with direct LLVM compilation.

### Q: Can I use S Lua for game development?
**A:** Yes! S Lua includes built-in Raylib support for graphics, 3D rendering, and UI. See `graphics/` examples.

### Q: Can I run S Lua code without compilation?
**A:** Currently no - you must compile. There's no interpreter or REPL. Future versions may add this.

---

## Compilation & Setup

### Q: Where do I get the compiler?
**A:** Build it yourself:
```bash
cd slua-compiler
cmake -B build -G Ninja && ninja -C build
```
The compiler will be at `build/compiler/sluac` (or `.exe` on Windows).

### Q: What do I need to compile S Lua code?
**A:**
- The S Lua compiler (sluac)
- LLVM/Clang toolchain (for linking)
- Standard C library

### Q: How do I run compiled S Lua code?
**A:** After compilation to `.ll` (LLVM IR), link with Clang:
```bash
sluac hello.slua -o hello.ll
clang hello.ll -o hello
./hello
```

Or use the convenience script: `./slua.ps1 Slua-Run hello.slua`

### Q: What's the difference between `.ll` and `.exe`?
**A:**
- `.ll` = LLVM Intermediate Representation (text format)
- `.exe` = Compiled machine code executable

---

## Language Features

### Q: What mode should I use - strict or nonstrict?
**A:** Use `strict` mode - it catches errors at compile time and generates better code. Use `nonstrict` only if you need dynamic typing.

### Q: What memory mode should I use?
**A:** Use `--!!mem:man` (manual) by default. This gives you full control. Use `--!!mem:auto` only to suppress warnings about manual memory operations.

### Q: Can I use classes or inheritance?
**A:** No native class system. Use records + methods pattern for OOP. See `advanced/oop.slua` for examples.

### Q: How do I handle errors?
**A:** Use `panic("error message")` to abort with an error message. There's no try-catch yet. Return error codes from functions for recoverable errors.

### Q: Can I use generics?
**A:** Generic types (`<T>`) are not implemented yet. Use `table` or `any` as a workaround.

### Q: Can I use regular expressions?
**A:** Yes, via `import regex`. See stdlib reference.

### Q: Do strings allocate memory?
**A:** Yes, string operations allocate but don't automatically free. Be careful in loops.

---

## Common Errors

### Q: What does "E0001 - unexpected token" mean?
**A:** Syntax error. Check for:
- Missing colons `:` in type declarations
- Missing commas `,` in lists/tables
- Unmatched brackets `{}` `()` `[]`
- Typos in keywords

### Q: What does "E0012 - undefined identifier" mean?
**A:** A variable, function, or type wasn't declared. Check:
- Variable is declared before use
- Function exists and is `export`ed if used in other modules
- Import statement is correct

### Q: What does "E0020 - no type and no initialiser" mean?
**A:** You declared a variable without a type OR initial value. Fix:
```lua
local x: int              -- OK, type provided
local y = 10              -- OK, type inferred from 10
local z                   -- ERROR - need one or both
```

### Q: What does "E0092 - type mismatch" mean?
**A:** Assigning wrong type. Example:
```lua
local x: int = "hello"    -- ERROR: can't assign string to int
local x: int = 42         -- OK
```

### Q: What does "E0050 - store target not a pointer" mean?
**A:** `store()` requires a pointer argument:
```lua
local x: int = 10
store(x, 42)              -- ERROR: x is not a pointer
store(addr(x), 42)        -- OK: addr(x) returns ptr<int>
```

### Q: Why does my defer statement not execute?
**A:** Known limitation - `defer` is parsed but doesn't fully run. Use `defer free()` for cleanup (works) but don't rely on `defer print()`. This is a compiler bug being fixed.

---

## Standard Library

### Q: How do I read a file?
**A:**
```lua
import fs

function main(): int
    local content: string = fs.read_all("file.txt")
    print(content)
    return 0
end
```

### Q: How do I make HTTP requests?
**A:**
```lua
import http

function main(): int
    local response: string = http.get("https://example.com")
    print(response)
    return 0
end
```

### Q: How do I parse JSON?
**A:**
```lua
import json

function main(): int
    local data: string = `{"name": "Alice", "age": 30}`
    local name: string = json.get_str(data, "name")
    print(name)  -- Alice
    return 0
end
```

### Q: How do I use random numbers?
**A:**
```lua
import random

function main(): int
    random.seed(42)
    local num: int = random.int(1, 100)
    print(num)
    return 0
end
```

### Q: How do I work with threads?
**A:** Thread support is limited. Use `import thread`:
```lua
import thread

function main(): int
    thread.sleep_ms(1000)       -- Sleep 1 second
    print("woke up")
    return 0
end
```
Full multithreading requires C callbacks.

---

## Performance & Optimization

### Q: Why is my program slow?
**A:** Check:
1. Are you compiling with optimizations? (`clang -O3`)
2. Are you allocating inside loops? Move allocation outside
3. Are you using table lookups in tight loops? Cache results
4. Did you profile to find the bottleneck?

### Q: How do I optimize memory usage?
**A:**
- Allocate once, reuse buffers
- Use `defer` for cleanup
- Avoid unnecessary table conversions
- Pre-allocate table sizes if known

### Q: What's faster - tables or pointers?
**A:** Pointers are faster for raw data. Tables are better for structured data.

---

## Integration & Advanced

### Q: Can I call C code from S Lua?
**A:** Yes, use `extern function`:
```lua
extern function strlen(s: string): int

function main(): int
    print(strlen("hello"))  -- 5
    return 0
end
```

### Q: Can I embed S Lua in a C program?
**A:** The compiled S Lua code links as normal LLVM - you can call it from C by exporting the `main` function or other functions.

### Q: Can I use Raylib for graphics?
**A:** Yes, S Lua has built-in Raylib support. See `graphics/demo_3d.slua`.

### Q: Can I create a shared library (.so / .dll)?
**A:** Not directly yet. You can compile to `.ll` and link as needed.

---

## Troubleshooting

### Q: The compiler crashes with no error message
**A:** 
- Try compiling with `--emit-tokens` to see if it's a parser issue
- Try `--emit-ast` to see if it's a semantic issue
- File a bug report with the source code

### Q: My program compiles but crashes at runtime
**A:** Check:
- Null pointer dereferences - add null checks
- Out-of-bounds memory access - check array indices
- Stack overflow - reduce recursion depth
- Use debugger: `gdb ./program` or `lldb ./program`

### Q: How do I debug my S Lua code?
**A:** 
1. Compile with debug info: Insert print statements
2. Use a debugger on the compiled binary: `gdb`, `lldb`
3. Check the generated `.ll` file for issues

### Q: Where do I report bugs?
**A:** Check the GitHub issues tracker or create a new issue.

---

## Getting Help

- **Documentation:** See [docs.md](docs.md) for full reference
- **Getting Started:** See [GETTING_STARTED.md](GETTING_STARTED.md)
- **Examples:** Check `examples/` directory
- **Community:** Post questions in project issues

---

*Last updated: April 12, 2026*
