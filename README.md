# S Lua

S Lua (Systems Lua) is a compiled systems programming language inspired by Lua syntax but designed for native performance and low-level control.

S Lua programs compile directly to native machine code using LLVM. There is no virtual machine and no garbage collector. Memory management is explicit and predictable, making the language suitable for systems software, game engines, tooling, and performance-critical applications.

File extension: `.slua`

---

# Design Goals

S Lua aims to combine the readability of Lua with the performance and control of systems languages.

Primary goals:

- Lua-style syntax
- Native machine code compilation
- No virtual machine
- No garbage collector
- Explicit memory control
- High performance
- Interoperability with C
- Static and dynamic typing support
- Modern compiler infrastructure

Strictly typed programs are designed to compile to code comparable to optimized C.

---

# Language Overview

S Lua syntax is inspired by Lua but introduces additional features for systems programming.

Major language features:

- static and dynamic typing
- strict and nonstrict compilation modes
- Lua-style tables
- generics
- union types
- optional types
- manual memory management
- pointer types
- compile-time type checking
- LLVM optimized native code generation

Example file extension:
---

# Basic Language Example

Strict mode example:

```lua
--!!strict

local x: int = 10
local y: int = 20

function add(a: int, b: int): int
    return a + b
end

print(add(x, y))
'''
Variables
Variables can be mutable or immutable.
'''lua 
--!!strict

local value: int = 10

const MAX_COUNT: int = 100
'''