
# S Lua

S Lua (Systems Lua, also referred to as Saksh's Lua) is a compiled systems programming language inspired by Lua syntax but designed for native performance and low level control.

Unlike standard Lua, S Lua does not run on a virtual machine and does not use a garbage collector. Programs compile directly to native machine code using LLVM.

The language aims to provide Lua style readability while giving programmers the control and performance expected from systems programming languages such as C.

Typical use cases include:

- systems tools
- game engines
- scripting for native applications
- performance critical utilities
- experimentation with language and compiler design

File extension used by S Lua programs:

.slua

---

# Design Goals

S Lua attempts to combine the simplicity of Lua with the power of modern compiler technology.

Primary goals:

- Lua style syntax
- native machine code compilation
- no virtual machine
- no mandatory garbage collector
- explicit memory control
- high performance
- interoperability with C style systems programming
- both strict and non strict typing
- modern compiler infrastructure

Strict mode programs are intended to produce code comparable to optimized C.

---

# Language Overview

S Lua syntax resembles Lua but introduces several features necessary for systems programming.

Major language features include:

- static typing
- non strict typing mode
- pointer types
- manual memory management
- generics
- optional types
- union types
- tables
- compile time directives
- LLVM optimized code generation

Example file extension:

example.slua

---

# Example Program

Basic strict mode example:

```slua
--!!strict

local x: int = 10
local y: int = 20

function add(a: int, b: int): int
    return a + b
end

print(add(x, y))
```

---

# Variables

Variables can be mutable or constant.

Example:

```slua
--!!strict

local value: int = 10

const MAX_COUNT: int = 100
```

---

# Functions

Functions support typed parameters and typed return values.

Example:

```slua
--!!strict

function multiply(a: int, b: int): int
    return a * b
end

print(multiply(4, 5))
```

---

# Memory Management

S Lua supports manual memory management for predictable performance.

Example:

```slua
--!!strict
--!!mem:man

function main(): int
    const N: int = 256

    local nums: ptr<int> = alloc_typed(int, N)

    if nums == null then
        panic("allocation failed")
    end

    defer free(nums)

    for i = 0, N - 1, 1 do
        store(nums + i, i * 2)
    end

    return 0
end
```

Memory functions:

- alloc_typed(type, count)
- free(pointer)
- store(pointer, value)
- deref(pointer)

---

# Compiler Directives

Files can contain directives at the top to configure compilation behavior.

Example:

```slua
--!!type:strict
--!!mem:man
```

Common directives include:

Type mode

--!!strict  
--!!nonstrict

Memory mode

--!!mem:man

---

# Compiler Architecture

The S Lua compiler follows a traditional multi stage compilation pipeline.

.slua source
    |
Lexer
    |
Parser
    |
Abstract Syntax Tree
    |
Semantic Analysis
    |
Type Checker
    |
LLVM IR Generation
    |
LLVM Optimization
    |
Native Machine Code

The compiler frontend is written in C++ and uses LLVM for backend code generation.

---

# Technologies Used

The S Lua compiler is built using the following technologies.

C++

The compiler frontend is implemented in C++ and handles:

- lexical analysis
- parsing
- semantic analysis
- code generation

LLVM

LLVM is used as the backend compiler infrastructure.

LLVM provides:

- intermediate representation
- optimization passes
- cross platform code generation
- machine code output

CMake

CMake is used to configure and build the compiler across different platforms.

---

# Installing Dependencies

Before building the compiler you must install several tools.

Required software:

- C++ compiler (MSVC, Clang, or GCC)
- CMake
- LLVM
- Clang

---

# Installing LLVM (Windows)

Download LLVM from the official website:

https://llvm.org/releases/

Install LLVM and ensure that the LLVM binaries are added to your system PATH.

During installation enable the option:

Add LLVM to the system PATH

After installation verify it works by running:

clang --version

If the command prints a version, LLVM is installed correctly.

---

# Building the Compiler

Clone the repository:

git clone https://github.com/Saksh3211/S-lua
cd S-lua

there are two ways from now (i recommend downloading the slua directly via repo or way 1) :-
1. direct and simple build:    
   .\cmake_configure.bat
   {this do all work at one}

2. mannual way -
    Create a build directory:
    
    mkdir build
    cd build
    
    Configure the project:
    
    cmake ..
    
    Build the compiler:
    
    cmake --build .
    
    This will generate the compiler executable:
    
    build/compiler/sluac.exe

---
# Running Slua code :
---
1. simple and easy way:
   .\slua.ps1 Slua-Run *.slua
   replace "*" with the path to your file
---
2. quickest way :
    just download the vscode extention and put paths to the compiler slua.ps1 file
    then restart vs code IDE and you will see a  run icon on top-right or press f5
    {make sure you have installed the proper softwares}
---
3. complex but flexible way:
    
    # Compiling a Program
    
    To compile a S Lua program into LLVM IR:
    
    .\build\compiler\sluac.exe examples\test_mine.slua -o examples\test_mine.ll
    
    This produces an LLVM IR file.
    
    # Generating an Executable
    
    Use clang to convert the LLVM IR file into a native executable.
    
    Example:
    
    clang examples\test_mine.ll build\runtime\slua.lib -o test_mine.exe
    
    This links the program with the S Lua runtime.
    
    # Running the Program
    
    Run the compiled executable:
    
    .\test_mine.exe

---

# Repository Structure

Typical repository structure:

S-lua/

compiler/
    include/
    src/

runtime/
    runtime library files

examples/
    example slua programs

build/
    build output

CMakeLists.txt
README.md
LICENSE

---

# Example Programs

Example programs are provided in the examples directory.

Examples include:

hello_strict.slua
hello_nonstrict.slua
math_test.slua
memory_example.slua
loop.slua
stdlib_demo.slua

These demonstrate language features and help test the compiler.

---

# License

This project is licensed under the MIT License.

See the LICENSE file for full details.
