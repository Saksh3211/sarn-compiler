# S Lua - Getting Started Guide

## Installation & Setup

### Prerequisites
- Windows, Linux, or macOS
- LLVM/Clang compiler (for linking compiled code)
- PowerShell (for running examples on Windows)

### Building the Compiler
```bash
cd slua-compiler
./cmake_configure.bat    # Windows
# or
cmake -B build -G Ninja && ninja -C build  # Linux/macOS
```

The compiler binary will be at `build/compiler/sluac.exe` (Windows) or `build/compiler/sluac` (Unix).

---

## Your First Program

### 1. Create a Simple File
Create `hello.slua`:
```lua
--!!type:strict

function main(): int
    print("Hello, S Lua!")
    return 0
end
```

### 2. Compile and Run
```powershell
.\slua.ps1 Slua-Run hello.slua
```

**Output:**
```
Hello, S Lua!
```

That's it! You've written your first S Lua program.

---

## Understanding the Directives

Every S Lua file starts with directives (line 1+):

```lua
--!!type:strict      # Enforce strict typing (or 'nonstrict')
--!!mem:man          # Manual memory management (or 'auto')
```

- **strict mode**: All variables must have explicit types, compile-time type checking
- **nonstrict mode**: Type inference, dynamic typing allowed, looser checks
- **manual memory (mem:man)**: You manage allocations with `alloc_typed()` and `free()`
- **auto memory (mem:auto)**: No manual memory warnings (experimental)

---

## Core Language Concepts

### Variables
```lua
local x: int = 10           -- Local mutable variable
const MAX: int = 100        -- Constant (immutable)
global counter: int = 0     -- Module-level global
```

### Types
```lua
local i: int = 42           -- Integer
local f: number = 3.14      -- Float (64-bit)
local s: string = "hello"   -- String
local b: bool = true        -- Boolean
local p: ptr<int> = addr(x) -- Pointer
```

### Functions
```lua
function add(a: int, b: int): int
    return a + b
end

-- Call it
local result: int = add(5, 3)
```

### Control Flow
```lua
if x > 0 then
    print("positive")
elseif x < 0 then
    print("negative")
else
    print("zero")
end

while x < 10 do
    print(x)
    x = x + 1
end

for i = 0, 9 do        -- Loop from 0 to 8 (exclusive end)
    print(i)
end
```

### Tables (Arrays & Maps)
```lua
-- Array
local nums: table = {1, 2, 3, 4, 5}
print(nums[1])              -- Access by index (1-based)

-- Map
local person: table = { name = "Alice", age = 30 }
print(person["name"])       -- Access by key

-- Add to table
table.push(nums, 6)         -- Requires: import table
```

---

## Common Patterns

### Pattern 1: Reading a File
```lua
--!!type:strict
import fs

function main(): int
    local content: string = fs.read_all("data.txt")
    print(content)
    return 0
end
```

### Pattern 2: Working with JSON
```lua
--!!type:strict
import json
import http

function main(): int
    local response: string = http.get("https://api.example.com/data")
    local user: string = json.get_str(response, "name")
    print(user)
    return 0
end
```

### Pattern 3: Memory Management
```lua
--!!type:strict
--!!mem:man

function main(): int
    local N: int = 1024
    local buffer: ptr<int> = alloc_typed(int, N)
    
    if buffer == null then
        panic("allocation failed")
    end
    
    defer free(buffer)  -- Cleanup when function exits
    
    -- Use buffer
    store(buffer, 42)
    local val: int = deref(buffer)
    print(val)
    
    return 0
end
```

### Pattern 4: Records (Structs)
```lua
--!!type:strict

type Person = { name: string, age: int }

function Person.new(name: string, age: int): Person
    return { name = name, age = age }
end

function Person.greet(self: Person): string
    return "Hello, I'm " .. self.name
end

function main(): int
    local alice: Person = Person.new("Alice", 30)
    print(Person.greet(alice))
    -- or with method syntax:
    print(alice:greet())
    return 0
end
```

### Pattern 5: Error Handling
```lua
--!!type:strict

function divide(a: int, b: int): int
    if b == 0 then
        panic("division by zero")
    end
    return a / b
end

function main(): int
    print(divide(10, 2))
    return 0
end
```

---

## Importing Modules

### Built-in Modules
```lua
import math      -- math.sqrt(), math.sin(), etc.
import string    -- string.upper(), string.lower(), etc.
import io        -- io.print(), io.read_line()
import fs        -- fs.read_all(), fs.write()
import json      -- JSON parsing
import http      -- HTTP requests
import random    -- Random numbers
import crypto    -- Hashing, encoding
```

### Custom Module (File Import)
**utils.slua:**
```lua
--!!type:strict

export function double(x: int): int
    return x * 2
end
```

**main.slua:**
```lua
--!!type:strict
import ("utils.slua")

function main(): int
    local result: int = double(21)
    print(result)  -- Output: 42
    return 0
end
```

---

## Learning Path

Follow these examples in order:

1. **basics/** - Start here
   - `variables.slua` - Declare variables, types
   - `functions.slua` - Create and call functions
   - `control_flow.slua` - if/else statements
   - `loops.slua` - for and while loops
   - `tables.slua` - Arrays and maps

2. **stdlib/** - Use the standard library
   - `math.slua` - Math functions
   - `io.slua` - Input/output
   - `fs_path.slua` - File operations
   - `json.slua` - JSON handling

3. **advanced/** - Advanced patterns
   - `memory.slua` - Manual memory management
   - `records.slua` - Define record types
   - `oop.slua` - Object-oriented patterns
   - `recursion.slua` - Recursive functions

4. **graphics/** - Graphics programming
   - `demo_3d.slua` - 3D rendering
   - `ui_demo.slua` - Interactive UI

---

## Debugging & Troubleshooting

### Compilation Errors

**Error E0001: Parse error / unexpected token**
```
[EE0001] main.slua:5:10  expected '}' in table literal, got ','
```
**Solution:** Check your syntax - missing or extra commas, brackets, parentheses.

**Error E0012: Undefined identifier**
```
[EE0012] main.slua:3:5  undefined identifier 'result'
```
**Solution:** Variable not declared or declared in wrong scope.

**Error E0092: Type mismatch**
```
[EE0092] main.slua:8:10  type mismatch: 'int' assigned to 'string'
```
**Solution:** Assigning wrong type - check variable declaration and assignment.

### Compiler Flags

```bash
sluac file.slua                     # Compile to output.ll
sluac file.slua -o out.ll           # Specify output file
sluac file.slua --emit-ast          # Dump abstract syntax tree
sluac file.slua --emit-tokens       # Dump token stream
sluac file.slua --strict            # Force strict mode
sluac file.slua --nonstrict         # Force nonstrict mode
```

---

## Performance Tips

1. **Use strict mode** - Catches errors early, generates better code
2. **Allocate buffers once** - Don't allocate inside loops
3. **Use tables for dynamic data** - More efficient than malloc'ing many small items
4. **Cache function results** - Avoid repeated expensive computations
5. **Use local variables** - Faster than global access

---

## Next Steps

- Read the full [Language Reference](docs.md)
- Explore example programs in `examples/`
- Join the community and ask questions
- Contribute improvements to the compiler!

---

## Quick Reference

| Concept | Example |
|---------|---------|
| Variable | `local x: int = 10` |
| Array | `local arr: table = {1, 2, 3}` |
| Function | `function add(a: int, b: int): int  return a + b  end` |
| Loop | `for i = 0, 10 do  print(i)  end` |
| Type | `type Point = { x: number, y: number }` |
| Pointer | `local p: ptr<int> = addr(x)` |
| Memory | `local buf = alloc_typed(int, 256)  defer free(buf)` |
| Import | `import math` or `import ("file.slua")` |
| String Concat | `"hello" .. " " .. "world"` |

Happy coding with S Lua!
