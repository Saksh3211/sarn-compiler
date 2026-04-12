
# S Lua

S Lua is a compiled systems programming language that compiles directly to native machine code via LLVM.

## Key Features

- Expressive syntax
- Compiled to native code (no VM, no GC)
- Manual memory management with full control
- Optional static typing (strict/nonstrict modes)
- Pointer types and manual memory control
- Modern compiler infrastructure (LLVM backend)
- Built-in Raylib for graphics and UI

## Quick Start

### Installation
```bash
cd slua-compiler
cmake -B build -G Ninja && ninja -C build
```

### Hello World
```lua
--!!type:strict

function main(): int
    print("Hello, S Lua!")
    return 0
end
```

Compile and run:
```bash
./slua.ps1 Slua-Run hello.slua
```

## Documentation

Start here based on your needs:

### Beginners
- **[GETTING_STARTED.md](GETTING_STARTED.md)** - Step-by-step intro and core concepts
- **[examples/basics/](examples/basics/)** - Starter code examples
- **[FAQ.md](FAQ.md)** - Common questions answered

### Developers
- **[docs.md](docs.md)** - Complete language reference
- **[PATTERNS.md](PATTERNS.md)** - Common idioms and design patterns
- **[examples/advanced/](examples/advanced/)** - Advanced language features
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Compiler error reference

### Reference
- **[Language Reference](docs.md)** - Syntax, operators, types, standard library
- **[Standard Library](docs.md#standard-library-modules)** - All built-in modules
- **[Error Codes](docs.md#error-codes)** - Compiler error reference

### Graphics & Games
- **[examples/graphics/](examples/graphics/)** - Raylib integration examples
- **[docs.md#gui](docs.md#gui--windowdrawinputuifontscene)** - Graphics library reference

## Learn S Lua

Examples are organized by increasing complexity:

### basics/ - Fundamentals (Start here!)
- `variables.slua` - Variable declaration and scope
- `types.slua` - Type system
- `functions.slua` - Function definition and calling
- `control_flow.slua` - if/else statements
- `loops.slua` - For and while loops
- `tables.slua` - Arrays and maps
- `operators.slua` - Arithmetic, comparison, logic
- And more...

### stdlib/ - Standard Library
- `math.slua` - Math functions (sqrt, sin, cos, etc.)
- `string.slua` - String manipulation
- `io.slua` - Input/output with colors
- `fs_path.slua` - Files and paths
- `json.slua` - JSON parsing
- `crypto.slua` - Hashing and encoding
- And more...

### advanced/ - Advanced Features
- `memory.slua` - Manual memory allocation
- `records.slua` - Record types (structs)
- `oop.slua` - Object-oriented patterns
- `error_handling.slua` - Panic and error recovery
- `recursion.slua` - Recursive algorithms
- And more...

### graphics/ - Graphics & UI
- `demo_3d.slua` - 3D rendering with Raylib
- `ui_demo.slua` - Interactive UI components
- `fonts.slua` - Font loading and rendering

## Language Highlights

### Static Typing with Inference
```lua
--!!type:strict

local x: int = 42           -- Explicit type
local y = 100               -- Type inference (int)
local z: string = "hello"   -- String type
```

### Memory Management
```lua
local buffer: ptr<int> = alloc_typed(int, 256)
defer free(buffer)          -- Automatic cleanup

store(buffer, 42)           -- Write to memory
local val: int = deref(buffer)  -- Read from memory
```

### Structs & Methods
```lua
type Person = { name: string, age: int }

function Person.greet(self: Person): string
    return "Hi, I'm " .. self.name
end

local alice: Person = { name = "Alice", age = 30 }
print(alice:greet())  -- Method call syntax
```

### Standard Library
```lua
import math, string, fs, json, http, crypto
```

Access to 20+ built-in modules including filesystem, networking, cryptography, JSON, regex, and more.

## Compilation Modes

### Strict Mode (Recommended)
```lua
--!!type:strict
```
Full type checking, compile-time error detection, best performance.

### Nonstrict Mode
```lua
--!!type:nonstrict
```
Type inference, dynamic typing, good for prototyping.

## Build Requirements

- C++ compiler (MSVC, Clang, GCC)
- CMake
- LLVM 18.1.6
- Clang

## Building

```powershell
.\cmake_configure.bat
```

This generates `build/compiler/sluac.exe` - the S Lua compiler.

## License

MIT License - See LICENSE file for details