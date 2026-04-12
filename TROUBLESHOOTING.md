# S Lua Compiler - Troubleshooting Guide

## Quick Diagnostic Checklist

When you encounter a compiler issue, follow this checklist:

1. Check file directives (line 1+): `--!!type:strict` and `--!!mem:man`
2. Check syntax: commas, colons, brackets, `end` keywords
3. Verify type declarations on all variables
4. Check function signatures match calls
5. Ensure `main()` function exists and returns `int`
6. Review the error code in the message

---

## Common Error Codes & Solutions

### E0001 - Parse Error / Unexpected Token

**Error Message Example:**
```
[EE0001] main.slua:5:10  unexpected token '}' in table literal
```

**Common Causes:**
- Missing commas in lists/tables
- Extra/missing parentheses or braces
- Invalid operator use
- Typo in keyword

**Solutions:**

Wrong:
```lua
local person: table = { name = "Alice" age = 30 }  -- Missing comma
```

Correct:
```lua
local person: table = { name = "Alice", age = 30 }
```

Wrong:
```lua
if x > 0
    print("positive")
end  -- Missing 'then'
```

Correct:
```lua
if x > 0 then
    print("positive")
end
```

---

### E0010 - Redeclaration in Strict Mode

**Error Message Example:**
```
[EE0010] main.slua:3:5  redeclaration of 'x' in strict mode
```

**Cause:** You declared the same variable twice in strict mode (nonstrict allows it).

**Solution:**

""" **Wrong:**
```lua
local x: int = 10
local x: int = 20  -- Can't redeclare
```

""" **Correct:**
```lua
local x: int = 10
x = 20              -- Assign instead of redeclare
```

Or in different scopes:
```lua
do
    local x: int = 10
    print(x)
end

do
    local x: int = 20  -- OK - different scope
    print(x)
end
```

---

### E0011 - Assignment to Const

**Error Message Example:**
```
[EE0011] main.slua:5:5  cannot assign to const 'MAX'
```

**Cause:** Trying to modify a `const` variable.

**Solution:**

""" **Wrong:**
```lua
const MAX: int = 100
MAX = 200  -- Can't change const
```

""" **Correct:**
```lua
local MAX: int = 100
MAX = 200              -- Use 'local' instead of 'const'
```

---

### E0012 - Undefined Identifier

**Error Message Example:**
```
[EE0012] main.slua:3:10  undefined identifier 'count'
```

**Common Causes:**
- Variable not declared
- Typo in variable/function name
- Missing import statement
- Variable declared in wrong scope

**Solutions:**

""" **Wrong:**
```lua
print(count)  -- 'count' not declared
```

""" **Correct:**
```lua
local count: int = 0
print(count)
```

""" **Wrong:**
```lua
local result: int = sqrt(9)  -- sqrt not imported
```

""" **Correct:**
```lua
import math
local result: number = math.sqrt(9)
```

---

### E0013 - Use of Uninitialised Variable

**Error Message Example:**
```
[EE0013] main.slua:5:10  use of possibly-uninitialized variable 'result'
```

**Cause:** Using a variable before assigning a value to it.

**Solutions:**

""" **Wrong:**
```lua
local x: int
print(x)  -- x is uninitialized
```

""" **Correct:**
```lua
local x: int = 0
print(x)
```

""" **Wrong:**
```lua
local value: string
if condition then
    value = "yes"
end
print(value)  -- value might not be set if condition is false
```

""" **Correct:**
```lua
local value: string = "no"  -- Provide default
if condition then
    value = "yes"
end
print(value)
```

---

### E0020 - No Type and No Initialiser

**Error Message Example:**
```
[EE0020] main.slua:2:5  declaration requires either type annotation or initialiser
```

**Cause:** Variable declared without type and without initial value.

**Solution:**

""" **Wrong:**
```lua
local x  -- No type, no value
```

""" **Correct (Option 1 - Explicit type):**
```lua
local x: int
```

""" **Correct (Option 2 - Type inference):**
```lua
local x = 42  -- Type inferred as int
```

---

### E0030 - Void Function Returns Value

**Error Message Example:**
```
[EE0030] main.slua:3:10  void function returns value
```

**Cause:** Function declared with no return type tries to return a value.

**Solution:**

""" **Wrong:**
```lua
function greet(): void
    return "hello"  -- Can't return value from void function
end
```

""" **Correct:**
```lua
function greet(): string
    return "hello"
end
```

---

### E0031 - Missing Return Value

**Error Message Example:**
```
[EE0031] main.slua:5:3  missing return value
```

**Cause:** Function has a return type but some code paths don't return.

**Solution:**

""" **Wrong:**
```lua
function absolute(x: int): int
    if x >= 0 then
        return x
    end
    -- Missing return for x < 0 case
end
```

""" **Correct:**
```lua
function absolute(x: int): int
    if x >= 0 then
        return x
    else
        return -x  -- Handle all cases
    end
end
```

---

### E0041 - For Range Not Numeric

**Error Message Example:**
```
[EE0041] main.slua:2:5  for loop range not numeric type
```

**Cause:** Using non-integer values in a for loop range.

**Solution:**

""" **Wrong:**
```lua
for i = "0", "10" do  -- Strings, not numbers
    print(i)
end
```

""" **Correct:**
```lua
for i = 0, 10 do
    print(i)
end
```

---

### E0050 - Store Target Not a Pointer

**Error Message Example:**
```
[EE0050] main.slua:5:5  store requires a pointer target
```

**Cause:** `store()` function requires a pointer, got something else.

**Solution:**

""" **Wrong:**
```lua
local x: int = 10
store(x, 42)  -- x is int, not ptr<int>
```

""" **Correct:**
```lua
local x: int = 10
store(addr(x), 42)  -- addr(x) returns ptr<int>
```

---

### E0051 - Free Requires Pointer

**Error Message Example:**
```
[EE0051] main.slua:8:5  free requires a pointer argument
```

**Cause:** Trying to free something that's not a pointer.

**Solution:**

""" **Wrong:**
```lua
local x: int = 10
free(x)  -- x is int, not pointer
```

""" **Correct:**
```lua
local buf: ptr<int> = alloc_typed(int, 256)
free(buf)
```

---

### E0070 - Call on Non-Function

**Error Message Example:**
```
[EE0070] main.slua:3:5  trying to call non-function value
```

**Cause:** Trying to call something that's not a function.

**Solution:**

""" **Wrong:**
```lua
local greeting: string = "hello"
greeting()  -- greeting is a string, not a function
```

""" **Correct:**
```lua
function greet(): string
    return "hello"
end

print(greet())  -- Call function, not variable
```

---

### E0071 - Wrong Argument Count

**Error Message Example:**
```
[EE0071] main.slua:5:5  function 'add' expects 2 arguments, got 3
```

**Cause:** Called function with wrong number of arguments.

**Solution:**

""" **Wrong:**
```lua
function add(a: int, b: int): int
    return a + b
end

local sum: int = add(1, 2, 3)  -- 3 arguments, but function expects 2
```

""" **Correct:**
```lua
local sum: int = add(1, 2)  -- Correct number of args
```

---

### E0092 - Type Mismatch

**Error Message Example:**
```
[EE0092] main.slua:5:10  type mismatch: 'string' assigned to 'int'
```

**Cause:** Assigning incompatible type.

**Solution:**

""" **Wrong:**
```lua
local count: int = "hello"  -- String can't go in int variable
```

""" **Correct:**
```lua
local count: int = 42      -- Assign int to int
local msg: string = "hello"  -- Or assign string to string
```

---

## Runtime Errors

### Segmentation Fault / Crash

**Cause:** Dereferencing null or invalid pointer.

**Solution:**

""" **Wrong:**
```lua
local p: ptr<int> = null
local x: int = deref(p)  -- Dereferencing null - crash!
```

""" **Correct:**
```lua
local p: ptr<int> = alloc_typed(int, 1)
defer free(p)

if p == null then panic("alloc failed") end
local x: int = deref(p)
```

### Stack Overflow

**Cause:** Too much recursion or local variable allocation.

**Solution:**

""" **Wrong:**
```lua
function infinite_recursion(): int
    return infinite_recursion() + 1  -- Never stops
end
```

""" **Correct:**
```lua
function countdown(n: int): int
    if n <= 0 then return 0 end
    return n + countdown(n - 1)
end
```

---

## Debugging Tips

### Enable Debug Output
Add print statements to trace execution:
```lua
function calculate(x: int): int
    print("calculate called with: " .. x)
    local result: int = x * 2
    print("result: " .. result)
    return result
end
```

### Use Optional Type Checking
Always check for null:
```lua
local ptr: ptr<int> = allocate_something()
if ptr == null then
    panic("allocation failed")
end
```

### Emit AST for Complex Issues
```bash
sluac problematic.slua --emit-ast > ast.txt
```
Inspect the output to see how your code is being parsed.

### Use External Debugger
For runtime crashes, use `gdb` or `lldb`:
```bash
gdb ./program
```

---

## Performance Debugging

### Check Generated IR
```bash
sluac slow_program.slua -o output.ll
# Review output.ll to see generated code
```

### Common Performance Issues

1. **Allocations in loops** - Move outside loop
2. **String operations** - Cache strings, avoid repeated parsing
3. **Table lookups** - Cache frequently accessed values
4. **Recursion** - Use iteration instead

---

## Getting Help

If you can't resolve the issue:

1. Isolate the problem - Create minimal reproduction
2. Check error code reference above
3. Review [docs.md](docs.md) for the feature
4. Check [GETTING_STARTED.md](GETTING_STARTED.md) for patterns
5. File issue with minimal code example

---

*Last updated: April 12, 2026*



