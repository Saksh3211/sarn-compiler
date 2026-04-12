# S Lua - Common Patterns & Best Practices

## Project Organization

### Recommended Directory Structure
```
my_project/
  """"""""" src/
  """   """"""""" main.slua
  """   """"""""" utils.slua
  """   """"""""" config.slua
  """   """"""""" types.slua
  """"""""" .packages/
  """   """"""""" (packages)
  """"""""" data/
  """   """"""""" (config files, resources)
```

### Module Organization Pattern
```lua
-- types.slua - Define all types
--!!type:strict

export type User = { id: int, name: string, email: string }
export type Config = { host: string, port: int }

-- utils.slua - Utility functions
--!!type:strict
import ("types.slua")

export function create_user(id: int, name: string, email: string): User
    return { id = id, name = name, email = email }
end

-- main.slua - Entry point
--!!type:strict
import ("types.slua")
import ("utils.slua")

function main(): int
    local user: User = create_user(1, "Alice", "alice@example.com")
    return 0
end
```

---

## Data Structures

### Stack Pattern
```lua
--!!type:strict

type Stack = { items: table, size: int }

function Stack.new(): Stack
    return { items = {}, size = 0 }
end

function Stack.push(self: Stack, val: int): void
    table.push(self.items, val)
    self.size = self.size + 1
end

function Stack.pop(self: Stack): int
    if self.size == 0 then
        panic("stack underflow")
    end
    self.size = self.size - 1
    return self.items[self.size]
end

function main(): int
    local stack: Stack = Stack.new()
    Stack.push(stack, 10)
    Stack.push(stack, 20)
    local val: int = Stack.pop(stack)
    print(val)  -- 20
    return 0
end
```

### Queue Pattern
```lua
--!!type:strict

type Queue = { items: table, front: int, rear: int }

function Queue.new(): Queue
    return { items = {}, front = 0, rear = 0 }
end

function Queue.enqueue(self: Queue, val: int): void
    table.push(self.items, val)
    self.rear = self.rear + 1
end

function Queue.dequeue(self: Queue): int
    if self.front >= self.rear then
        panic("queue empty")
    end
    self.front = self.front + 1
    return self.items[self.front - 1]
end

function main(): int
    local q: Queue = Queue.new()
    Queue.enqueue(q, 10)
    Queue.enqueue(q, 20)
    local x: int = Queue.dequeue(q)
    print(x)  -- 10
    return 0
end
```

### Linked List Node Pattern
```lua
--!!type:strict

type Node = { value: int, next: ptr<Node> }

function Node.new(value: int): ptr<Node>
    local n: ptr<Node> = alloc_typed(Node, 1)
    if n == null then panic("alloc failed") end
    store(n, { value = value, next = null })
    return n
end

function main(): int
    local head: ptr<Node> = Node.new(1)
    defer free(head)
    
    local n2: ptr<Node> = Node.new(2)
    defer free(n2)
    
    -- Link them
    local h: Node = deref(head)
    h.next = n2
    store(head, h)
    
    return 0
end
```

---

## Error Handling

### Pattern 1: Panic on Critical Errors
```lua
--!!type:strict
import fs

function main(): int
    local content: string = fs.read_all("critical.txt")
    if content == "" then
        panic("critical file missing")
    end
    process(content)
    return 0
end

function process(data: string): void
    print(data)
end
```

### Pattern 2: Return Error Codes
```lua
--!!type:strict
import fs

function safe_read(path: string): int
    local content: string = fs.read_all(path)
    if content == "" then
        return 1  -- Error code
    end
    process(content)
    return 0  -- Success
end

function main(): int
    return safe_read("data.txt")
end

function process(data: string): void
    print(data)
end
```

### Pattern 3: Try-Catch Simulation (using defer)
```lua
--!!type:strict
--!!mem:man

function process_with_cleanup(): int
    local buffer: ptr<uint8> = alloc_typed(uint8, 1024)
    if buffer == null then return 1 end
    defer free(buffer)
    
    -- Process...
    return 0
end

function main(): int
    return process_with_cleanup()
end
```

---

## Memory Management

### Pattern 1: Defer for Cleanup
```lua
--!!type:strict
--!!mem:man

function process_file(path: string): int
    local handle: int = fs.open(path, "r")
    if handle == 0 then return 1 end
    defer fs.close(handle)
    
    local line: string = fs.readline(handle)
    print(line)
    
    return 0
end
```

### Pattern 2: Allocate Once, Reuse Many Times
```lua
--!!type:strict
--!!mem:man

function process_large_dataset(n: int): int
    -- Allocate buffer once
    local buffer: ptr<number> = alloc_typed(number, n)
    if buffer == null then panic("alloc failed") end
    defer free(buffer)
    
    -- Reuse across iterations
    for i = 0, n, 1 do
        local idx: int = cast(int, i)
        store(ptr_cast(ptr<number>, cast(int, buffer) + idx * sizeof(number)), cast(number, i))
    end
    
    return 0
end
```

### Pattern 3: Buffer Pooling (Simulate)
```lua
--!!type:strict

type BufferPool = { buffers: table, available: int }

function BufferPool.new(size: int, count: int): BufferPool
    local pool: BufferPool = { buffers = {}, available = count }
    return pool
end

function main(): int
    local pool: BufferPool = BufferPool.new(256, 10)
    return 0
end
```

---

## String Handling

### Pattern 1: Building Strings Efficiently
```lua
--!!type:strict

function build_message(parts: table): string
    local result: string = ""
    
    -- Concatenate all parts
    for i = 1, #parts do
        result = result .. parts[i] .. " "
    end
    
    return result
end

function main(): int
    local parts: table = {"Hello", "from", "S", "Lua"}
    print(build_message(parts))
    return 0
end
```

### Pattern 2: String Parsing
```lua
--!!type:strict
import string

function parse_line(line: string): int
    local trimmed: string = string.trim(line)
    if trimmed == "" then return 0 end
    
    local idx: int = string.find(trimmed, ":", 0)
    if idx == -1 then return 0 end
    
    local key: string = string.sub(trimmed, 0, idx)
    local value: string = string.sub(trimmed, idx + 1, #trimmed)
    
    print("Key: " .. key)
    print("Value: " .. value)
    return 1
end

function main(): int
    return parse_line("name:Alice")
end
```

---

## Functional Patterns

### Pattern 1: Function as Parameter
```lua
--!!type:strict

function apply_to_all(values: table, fn: (int) -> int): table
    local result: table = {}
    
    for i = 1, #values do
        local transformed: int = fn(values[i])
        table.push(result, transformed)
    end
    
    return result
end

function double(x: int): int
    return x * 2
end

function main(): int
    local nums: table = {1, 2, 3}
    local doubled: table = apply_to_all(nums, double)
    return 0
end
```

### Pattern 2: Higher-Order Functions
```lua
--!!type:strict

function compose(f: (int) -> int, g: (int) -> int, x: int): int
    return f(g(x))
end

function square(x: int): int
    return x * x
end

function double(x: int): int
    return x * 2
end

function main(): int
    -- Compose: square(double(5)) = 100
    local result: int = compose(square, double, 5)
    print(result)
    return 0
end
```

---

## Iteration Patterns

### Pattern 1: Iterating Over Table
```lua
--!!type:strict

function process_table(data: table): void
    for i = 1, #data do
        print(data[i])
    end
end

function main(): int
    local items: table = {10, 20, 30}
    process_table(items)
    return 0
end
```

### Pattern 2: While Loop with Index
```lua
--!!type:strict

function process_until_end(items: table): void
    local i: int = 1
    while i <= #items do
        print(items[i])
        i = i + 1
    end
end

function main(): int
    local items: table = {1, 2, 3}
    process_until_end(items)
    return 0
end
```

### Pattern 3: Early Exit
```lua
--!!type:strict

function find_value(items: table, target: int): int
    for i = 1, #items do
        if items[i] == target then
            return i
        end
    end
    return -1
end

function main(): int
    local items: table = {10, 20, 30}
    local idx: int = find_value(items, 20)
    print(idx)  -- 2
    return 0
end
```

---

## Config & Initialization

### Pattern 1: Config Type
```lua
--!!type:strict
import json
import fs

export type Config = { 
    host: string, 
    port: int, 
    debug: int,
    workers: int 
}

export function load_config(path: string): Config
    local content: string = fs.read_all(path)
    if content == "" then
        return default_config()
    end
    
    return {
        host = json.get_str(content, "host"),
        port = json.get_int(content, "port"),
        debug = json.get_bool(content, "debug"),
        workers = json.get_int(content, "workers")
    }
end

function default_config(): Config
    return {
        host = "localhost",
        port = 8080,
        debug = 0,
        workers = 4
    }
end

function main(): int
    local cfg: Config = load_config("config.json")
    print(cfg.host)
    return 0
end
```

### Pattern 2: Singleton Initialization
```lua
--!!type:strict
--!!mem:man

global _initialized: int = 0
global _state: ptr<uint8> = null

function init(): int
    if _initialized == 1 then return 0 end
    
    _state = alloc_typed(uint8, 1024)
    if _state == null then return 1 end
    
    _initialized = 1
    return 0
end

function cleanup(): void
    if _initialized == 1 and _state ~= null then
        free(_state)
        _initialized = 0
    end
end

function main(): int
    if init() ~= 0 then return 1 end
    defer cleanup()
    
    return 0
end
```

---

## Performance Patterns

### Pattern 1: Caching Computed Values
```lua
--!!type:strict

type Result = { computed: number, cached: int }

function expensive_computation(): number
    return 3.14159
end

function main(): int
    local pi: number = expensive_computation()  -- Compute once
    
    -- Use cached value multiple times
    local area1: number = pi * 5 * 5
    local area2: number = pi * 10 * 10
    
    return 0
end
```

### Pattern 2: Batch Processing
```lua
--!!type:strict

function process_batch(items: table, batch_size: int): void
    local i: int = 1
    while i <= #items do
        -- Process batch_size items
        local end_idx: int = i + batch_size
        if end_idx > #items then
            end_idx = #items
        end
        
        -- Process batch
        i = end_idx + 1
    end
end

function main(): int
    local items: table = {1, 2, 3, 4, 5}
    process_batch(items, 2)
    return 0
end
```

---

## Testing Patterns

### Pattern 1: Simple Assertions
```lua
--!!type:strict

function assert(condition: int, message: string): void
    if condition == 0 then
        panic(message)
    end
end

function test_addition(): void
    local result: int = 2 + 2
    assert(result == 4, "Addition test failed")
end

function main(): int
    test_addition()
    print("All tests passed")
    return 0
end
```

### Pattern 2: Unit Test Structure
```lua
--!!type:strict

function test_suite_1(): int
    -- Test 1
    if run_check_1() ~= 0 then return 1 end
    -- Test 2
    if run_check_2() ~= 0 then return 1 end
    return 0
end

function run_check_1(): int
    if 1 + 1 ~= 2 then return 1 end
    return 0
end

function run_check_2(): int
    if 2 * 3 ~= 6 then return 1 end
    return 0
end

function main(): int
    if test_suite_1() == 0 then
        print("All tests passed")
        return 0
    else
        print("Tests failed")
        return 1
    end
end
```

---

## Best Practices

### DO:
1. Use `strict` mode for type safety
2. Initialize all variables before use
3. Check for null pointers
4. Use `defer` for resource cleanup
5. Document complex types with comments
6. Separate concerns into modules
7. Use meaningful variable names
8. Handle errors explicitly

### DON'T:
1. Mix strict and nonstrict code
2. Allocate inside tight loops
3. Forget to free allocated memory
4. Dereference potentially null pointers
5. Use magic numbers - define constants
6. Create deeply nested code - refactor to functions
7. Ignore compiler warnings
8. Leave TODO comments in productions code

---

*Last updated: April 12, 2026*

