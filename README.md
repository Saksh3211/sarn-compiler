
# S Lua

S Lua is a compiled systems programming language with Lua-like syntax that compiles directly to native machine code via LLVM.

## Key Features

- Lua-inspired syntax
- Compiled to native code (no VM, no GC)
- Manual memory management
- Optional static typing (strict/nonstrict modes)
- Pointer types and manual memory control
- Modern compiler infrastructure

## Quick Start

```slua
--!!type:nonstrict

function main()
    local x = 10
    local y = 20
    print(x + y)
    return 0
end
```

Compile and run:
```powershell
.\slua.ps1 Slua-Run examples/01_basics/hello_nonstrict.slua
```

## Learn S Lua

Examples are organized by complexity:

- **01_basics/** - Fundamental language features (variables, loops, functions)
- **02_stdlib/** - Built-in libraries (math, string, io, crypto, etc)
- **03_advanced/** - Advanced features (OOP, enums, memory management, reflection)
- **04_graphics/** - Graphics and UI with Raylib integration
- **cryptoApp/** - Real-world application example

Start with `01_basics/hello_nonstrict.slua` and explore examples in order.

## Documentation

See [docs.md](docs.md) for detailed language reference and feature documentation.


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

