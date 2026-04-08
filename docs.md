# S Lua Language Reference

## File directives

Every `.slua` file should start with directives on line 1+.
```
--!!type:strict
--!!type:nonstrict
--!!mem:man
--!!mem:auto
```

`strict` enforces full type checking and errors on violations.
`nonstrict` allows untyped variables and looser checks.

`mem:man` is manual memory management (default).
`mem:auto` disables manual memory warnings.

---

## Program Entry Point

Program execution starts at the `main` function:
```lua
function main(): int
    -- your code here
    return 0
end
```

The `main` function must exist and returns an exit code (0 for success).
You can call other functions and import modules before defining `main`.

---

## Comments
```lua
-- single line comment
--[[ multi
     line comment ]]
```

---

## Primitive types

| Type | Description |
|------|-------------|
| `int` | 64-bit signed integer (alias: int64, int32, int16, int8) |
| `number` | 64-bit double float (alias: float, double) |
| `string` | Immutable UTF-8 string |
| `bool` | true or false |
| `void` | No value (return only) |
| `any` | Dynamic untyped value |
| `null` | Null pointer literal |
| `uint8` `uint16` `uint32` `uint64` | Unsigned integers |
| `char` `byte` | Aliases for int8/uint8 |

---

## Literals
```lua
42          -- int
0xFF        -- hex int (= 255)
0b1010      -- binary int (= 10)
3.14        -- number
"hello"     -- string
'world'     -- string
true false  -- bool
null        -- null pointer
```

---

## Variables
```lua
local x: int = 10           -- mutable local
local y = 20                -- type inferred
const MAX: int = 100        -- immutable constant
global counter: int = 0     -- module-level global
```

`const` requires an initialiser and cannot be reassigned.
`global` lives for the lifetime of the program.

---

## Operators

### Arithmetic
```
+  -  *  /  %
```
Division `/` always returns `number`.
Use cast to get integer division: `cast(int, a / b)`.

### Comparison
```
==  ~=  <  >  <=  >=
```

### Logic
```
and  or  not
```
Short-circuit: `and` returns rhs if lhs is truthy, else lhs.

### String concat
```lua
"hello" .. " " .. "world"
```

### Bitwise
```
&  |  ~  <<  >>
```
Operands must be `int`.

### Unary
```
-x      -- negate
not x   -- boolean not
#t      -- length of table or string
```

---

## Type annotations
```lua
local x: int
local p: ptr<int>
local f: (int, int) -> int
local opt: int?             -- optional (can be null)
local u: int | string       -- union type
```

---

## Functions
```lua
function add(a: int, b: int): int
    return a + b
end

-- multiple return values
function divmod(a: int, b: int): int, int
    return a / b, a % b
end

local q, r = divmod(17, 5)

-- anonymous function
local square = function(x: int): int
    return x * x
end

-- exported (visible to other files via import)
export function greet(name: string): string
    return "Hello, " .. name
end

-- extern C function
extern function strlen(s: string): int
```

Function names with a dot define methods on a type:
```lua
function Vec2.len(self: Vec2): number
    return math.sqrt(self.x*self.x + self.y*self.y)
end
```

---

## Control flow

### if
```lua
if x > 0 then
    print("positive")
elseif x < 0 then
    print("negative")
else
    print("zero")
end
```

### while
```lua
local i: int = 0
while i < 10 do
    print(i)
    i = i + 1
end
```

### repeat-until
```lua
repeat
    i = i + 1
until i >= 10
```

### numeric for
```lua
for i = 0, 9, 1 do
    print(i)
end
-- step is optional, defaults to 1
-- start (0) is inclusive, end (9) is EXCLUSIVE — loop runs 0..8
-- FIXED: Now correctly uses exclusive end comparison
```

### C-style for
```lua
for i = 0, i < 10, i + 1 do
    print(i)
end
```

### break / continue
```lua
while true do
    if done == 1 then break end
    if skip == 1 then continue end
end
```

### do block (scoped)
```lua
do
    local tmp: int = expensive()
end
-- tmp not accessible here
```

### defer
```lua
function read_file(path: string): string
    local f: int = fs.open(path, "r")
    defer fs.close(f)
    return fs.readline(f)
end
```
`defer` runs the statement when the enclosing function exits, in reverse order.

---

## Tables

Tables are the universal container — arrays, maps, and object bases.
```lua
-- array style
local nums: table = {1, 2, 3, 4, 5}

-- map style
local person: table = { name = "Alice", age = 30 }

-- mixed
local mixed: table = { 10, 20, x = 99 }

-- read
local v: int = nums[1]
local n: string = person["name"]

-- write
nums[6] = 60
person["city"] = "London"

-- length
local len: int = #nums
```

### Table standard library (`import table`)
```lua
table.push(t, val)          -- append int
table.push_float(t, val)    -- append number
table.push_str(t, val)      -- append string
table.pop(t)                -- remove last
table.len(t)                -- length (same as #t)
table.remove_at(t, idx)     -- remove by 1-based index
table.clear(t)              -- empty the table
table.reverse(t)            -- reverse in place
table.keys(t)               -- newline-separated key list string
table.contains_int(t, val)  -- 1 if val exists
table.contains_str(t, val)  -- 1 if val exists
table.merge(a, b)           -- new table: a + b
table.slice(t, from, to)    -- new table: sub-range (1-based)
table.new()                 -- create empty table
```

---

## Record types (structs)
```lua
type Vec3 = { x: number, y: number, z: number }

-- constructor pattern
function Vec3.new(x: number, y: number, z: number): Vec3
    return { x = x, y = y, z = z }
end

-- method pattern (self is first parameter)
function Vec3.len(self: Vec3): number
    return math.sqrt(self.x*self.x + self.y*self.y + self.z*self.z)
end

-- usage
local v: Vec3 = Vec3.new(1.0, 2.0, 3.0)
local l: number = Vec3.len(v)
-- or method call syntax:
local l2: number = v:len()
```

Field access:
```lua
v.x = 5.0
local y: number = v.y
```

---

## Enums
```lua
enum Direction = { UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3 }
enum Color = { RED, GREEN, BLUE }   -- auto values 0 1 2

local d: int = UP
if d == DOWN then print("going down") end
```

Enum members are `int` constants in the enclosing scope.

---

## Generics
```lua
type Stack<T> = { data: ptr<T>, size: int, cap: int }

function Stack.new<T>(cap: int): Stack<T>
    return { data = alloc_typed(T, cap), size = 0, cap = cap }
end

function Stack.push<T>(s: Stack<T>, val: T): void
    store(s.data + s.size, val)
    s.size = s.size + 1
end
```

---

## Memory management
```lua
-- allocate N elements of type T
local buf: ptr<int> = alloc_typed(int, 256)

-- check for null
if buf == null then panic("alloc failed") end

-- always free with defer
defer free(buf)

-- write to pointer offset
store(buf + i, value)

-- read from pointer
local v: int = deref(buf + i)

-- get raw address
local p: ptr<int> = addr(my_var)

-- type cast
local u8: uint8 = cast(uint8, some_int)

-- pointer cast
local raw: ptr<uint8> = ptr_cast(ptr<uint8>, typed_ptr)

-- size of type in bytes
local sz: int = sizeof(int)
```

---

## panic
```lua
panic("error message")   -- abort with message, prints file and line
```

---

## File imports
```lua
import math               -- built-in module
import ("utils.slua")     -- file import (path relative to current file)
```

File imports inline all declarations from the target file before the current file compiles. Imported files must also have `--!!type` directives.

---

## Packages and Module System

### Using Built-in Packages

S Lua provides built-in standard library packages that are imported by name:
```lua
import math
import string
import fs
import http
import json
-- ... and many others
```

These are compiled standard library modules provided by the S Lua runtime.

### Creating and Using Custom Packages (Modules)

You can create reusable modules by splitting code into separate `.slua` files. Each file acts as a package:

**mymath.slua:**
```lua
--!!type:strict

function add(a: int, b: int): int
    return a + b
end

export function multiply(a: int, b: int): int
    return a * b
end
```

**main.slua:**
```lua
--!!type:strict
import ("mymath.slua")

function main(): int
    -- add is not exported, only accessible within mymath.slua
    -- multiply is exported, but accessed via the import
    print(multiply(3, 4))
    return 0
end
```

### Export and Visibility

- Functions declared with `export` are visible to importing files
- Functions without `export` are private to their module
- All `type` and `enum` declarations are accessible by default across modules
- Imported files must have matching `--!!type` directives

### Package Registration

The S Lua runtime uses a module registry system. Built-in packages are registered at startup and can be imported without file paths. External C modules can be registered programmatically to extend the runtime.

---

## OOP pattern

S Lua uses a static dispatch OOP pattern — no vtables, no inheritance overhead.
```lua
-- define type
type Animal = { name: string, sound: string, legs: int }

-- constructor
function Animal.new(name: string, sound: string, legs: int): Animal
    return { name = name, sound = sound, legs = legs }
end

-- methods
function Animal.speak(self: Animal): string
    return self.name .. " says " .. self.sound
end

function Animal.describe(self: Animal): string
    return self.name .. " has " .. self.legs .. " legs"
end

-- usage
local dog: Animal = Animal.new("Dog", "woof", 4)
print(Animal.speak(dog))
print(dog:describe())        -- method call syntax
```

Methods can also be called with `:` syntax:
```lua
v:len()          -- same as Vec3.len(v)
dog:speak()      -- same as Animal.speak(dog)
```

---

## Standard library modules

### math (`import math`)
```lua
math.sqrt(x)      math.pow(b, e)    math.sin(x)
math.cos(x)       math.tan(x)       math.log(x)
math.log2(x)      math.exp(x)       math.inf()
math.nan()        math.pi()         math.e()          -- pi and e are function calls
math.clamp(v, lo, hi)              -- clamp value to range
math.lerp(a, b, t)                 -- linear interpolation
math.min2(a, b)   math.max2(a, b)  -- min/max of two values
math.abs(x)       math.floor(x)     math.ceil(x)    math.round(x)
math.sign(x)      math.fract(x)     math.mod(a, b)
```

### io (`import io`)
```lua
io.read_line()              -- string from stdin
io.read_char()              -- int byte from stdin
io.print(s)                 -- print without newline
io.print_color(s, color)    -- color: "red" "green" "yellow" "blue" "cyan" "magenta" "white"
io.set_color(color)
io.reset_color()
io.clear()
io.flush()
```

### os (`import os`)
```lua
os.time()                   -- unix timestamp int
os.sleep(ms)                -- sleep milliseconds
os.sleepS(s)                -- sleep seconds
os.getenv(key)              -- string
os.system(cmd)              -- run shell command
os.cwd()                    -- current directory string
```

### string (`import string`)
```lua
string.len(s)
string.upper(s)         string.lower(s)
string.sub(s, from, to) -- 0-based inclusive
string.byte(s, i)       -- byte at index
string.char(b)          -- byte to string
string.find(s, needle, from)   -- -1 if not found
string.trim(s)
string.concat(a, b)
string.to_int(s)
string.to_float(s)
```

### stdata (`import stdata`)
```lua
stdata.typeof(v)        -- "int" "number" "bool" "string" "null"
stdata.tostring(v)
stdata.tointeger(v)
stdata.tofloat(v)
stdata.tobool(v)
stdata.isnull(v)
stdata.assert(cond, msg)
```

### fs (`import fs`)
```lua
fs.read_all(path)           -- string
fs.write(path, data)        -- int (1=ok)
fs.append(path, data)
fs.exists(path)             -- int
fs.delete(path)
fs.mkdir(path)
fs.rename(from, to)
fs.size(path)               -- int bytes
fs.listdir(path)            -- newline-separated names
fs.copy(src, dst)
-- file handles
local h: int = fs.open(path, "r")   -- "r" "w" "a" "rb" "wb"
fs.readline(h)              -- string
fs.writeh(h, data)
fs.flush(h)
fs.close(h)
```

### path (`import path`)
```lua
path.join(a, b)
path.basename(p)    -- "file.slua"
path.dirname(p)     -- "examples/portfolio"
path.extension(p)   -- ".slua"
path.stem(p)        -- "file"
path.absolute(p)
path.normalize(p)
path.exists(p)      -- int
path.is_file(p)     -- int
path.is_dir(p)      -- int
```

### json (`import json`)
```lua
json.get_str(resp, key)
json.get_int(resp, key)
json.get_float(resp, key)
json.get_bool(resp, key)
json.has_key(resp, key)
json.get_array_item(resp, key, index)
json.encode_str(s)
json.encode_int(n)
json.encode_float(f)
json.encode_bool(b)
json.encode_null()
json.minify(s)
```

### http (`import http`)
```lua
http.get(url)                       -- string response body
http.post(url, body, content_type)
http.post_json(url, json_body)
http.status(url)                    -- int HTTP status code
```
Uses WinHTTP on Windows, raw sockets on Linux/macOS.

### net (`import net`)
```lua
net.init()
net.connect(host, port)     -- int socket id
net.listen(port)            -- int server socket id
net.accept(server_id)       -- int client socket id
net.send(id, data)
net.recv(id, maxlen)        -- string
net.send_bytes(id, data, len)
net.close(id)
net.local_ip()              -- string
```

### random (`import random`)
```lua
random.seed(n)
random.int(lo, hi)          -- int in [lo, hi]
random.float()              -- number in [0.0, 1.0)
random.range(lo, hi)
random.gauss(mean, stddev)
```

### datetime (`import datetime`)
```lua
datetime.now()              -- unix timestamp int
datetime.now_str(fmt)       -- formatted string e.g. "%Y-%m-%d %H:%M:%S"
datetime.format(ts, fmt)
datetime.parse(str, fmt)    -- int timestamp
datetime.diff(a, b)         -- int seconds
datetime.add(ts, seconds)
datetime.year(ts)    datetime.month(ts)   datetime.day(ts)
datetime.hour(ts)    datetime.minute(ts)  datetime.second(ts)
```

### process (`import process`)
```lua
process.run(cmd)            -- int exit code
process.output(cmd)         -- string stdout
process.spawn(cmd)          -- int process id
process.wait(id)            -- int exit code
process.kill(id)
process.alive(id)           -- int
```

### sync (`import sync`)
```lua
local m: int = sync.mutex_new()
sync.lock(m)
sync.unlock(m)
sync.trylock(m)             -- int 1 if acquired
sync.free(m)
```

### thread (`import thread`)
```lua
thread.sleep_ms(ms)
thread.self_id()            -- int
thread.join(id)
thread.detach(id)
thread.alive(id)            -- int
```
Note: thread.create requires a C function pointer — use `extern function` + native callbacks for full threading.

### crypto (`import crypto`)
```lua
crypto.sha256(data)                     -- hex string
crypto.md5(data)                        -- hex string
crypto.hmac_sha256(key, data)           -- hex string
crypto.crc32(data, len)                 -- int
crypto.base64_encode(data, len)         -- string
crypto.base64_decode(b64)               -- string
crypto.hex_encode(data, len)            -- string
crypto.hex_decode(hex)                  -- string
crypto.xor(data, len, key, keylen)      -- string
```

### buf (`import buf`)
```lua
local b: int = buf.new(size)               -- create buffer
local b: int = buf.from_str(s, len)        -- create from string
buf.write_u8(b, offset, val)
buf.write_u16(b, offset, val)
buf.write_u32(b, offset, val)
buf.write_i64(b, offset, val)
buf.write_f32(b, offset, val)
buf.write_f64(b, offset, val)
buf.write_str(b, offset, s)
buf.read_u8(b, offset)
buf.read_u16(b, offset)
buf.read_u32(b, offset)
buf.read_i64(b, offset)
buf.read_f32(b, offset)     -- number
buf.read_f64(b, offset)     -- number
buf.to_str(b)
buf.to_hex(b)
buf.fill(b, offset, len, val)
buf.copy(dst, doff, src, soff, len)
buf.size(b)                 -- int
buf.free(b)
```

### regex (`import regex`)
```lua
regex.match(str, pattern)              -- int: 1 if matches, 0 if not
regex.find(str, pattern, from)         -- int: position of first match (-1 if none)
regex.replace(str, pattern, repl)      -- string: replace all matches
regex.groups(str, pattern)             -- extract regex capture groups
regex.count(str, pattern)              -- int: count total matches
regex.find_all(str, pattern)           -- find all match positions
```

### vec (`import vec`)
```lua
vec.v2_dot(ax,ay, bx,by)
vec.v2_len(x, y)
vec.v2_dist(ax,ay, bx,by)
vec.v2_norm_x(x, y)
vec.v2_norm_y(x, y)
vec.v3_dot(ax,ay,az, bx,by,bz)
vec.v3_len(x, y, z)
vec.v3_dist(ax,ay,az, bx,by,bz)
vec.v3_norm_x(x,y,z)   vec.v3_norm_y(x,y,z)   vec.v3_norm_z(x,y,z)
vec.v3_cross_x(ax,ay,az, bx,by,bz)
vec.v3_cross_y(ax,ay,az, bx,by,bz)
vec.v3_cross_z(ax,ay,az, bx,by,bz)
vec.clamp(v, lo, hi)
vec.lerp(a, b, t)
vec.abs(x)    vec.floor(x)   vec.ceil(x)   vec.round(x)
vec.min(a,b)  vec.max(a,b)   vec.sign(x)
vec.fract(x)  vec.mod(a,b)
```

### GUI — window/draw/input/ui/font/scene (`import stdgui`)

#### window
```lua
window.init(w, h, title, fps)
window.close()
window.should_close()       -- int
window.begin_drawing()
window.end_drawing()
window.clear(r, g, b, a)
window.set_fps(fps)
window.get_fps()            -- int
window.frame_time()         -- number
window.width()              window.height()
```

#### draw
```lua
draw.rect(x, y, w, h, r, g, b, a)
draw.rect_outline(x, y, w, h, thick, r, g, b, a)
draw.circle(cx, cy, radius, r, g, b, a)
draw.circle_outline(cx, cy, radius, r, g, b, a)
draw.line(x1, y1, x2, y2, thick, r, g, b, a)
draw.triangle(x1,y1, x2,y2, x3,y3, r, g, b, a)
draw.text(text, x, y, size, r, g, b, a)
draw.measure_text(text, size)   -- int width
draw.text_font(fid, text, x, y, size, spacing, r, g, b, a)
```

#### input
```lua
input.key_down(key)         -- int
input.key_pressed(key)      -- int (once per press)
input.key_released(key)     -- int
input.mouse_x()   input.mouse_y()
input.mouse_pressed(btn)    -- 0=left 1=right 2=middle
input.mouse_down(btn)
input.mouse_wheel()         -- number
```

Common key codes: ESC=256, SPACE=32, ENTER=257, A-Z = 65-90, 0-9 = 48-57.
Arrow keys: RIGHT=262, LEFT=263, DOWN=264, UP=265.

#### ui
```lua
ui.button(x, y, w, h, label)           -- int 1 if clicked
ui.label(x, y, w, h, text)
ui.checkbox(x, y, size, text, checked) -- int new state
ui.slider(x, y, w, h, min, max, val)   -- number new value
ui.progress_bar(x, y, w, h, val, max)
ui.panel(x, y, w, h, title)
ui.text_input(x, y, w, h, buf, bufsize, active) -- int active
ui.set_font_size(size)
ui.set_accent(r, g, b)
```

#### font
```lua
local fid: int = font.load(path, size)
font.unload(fid)
-- draw with: draw.text_font(fid, text, x, y, size, spacing, r, g, b, a)
```

#### 3D scene (`import stdgui`)
```lua
scene.window_init_3d(w, h, title)
scene.camera_set(px,py,pz, tx,ty,tz, ux,uy,uz, fovy, proj)
scene.camera_update()
scene.begin()
scene.end()
scene.grid(slices, spacing)
scene.cube(x,y,z, w,h,d, r,g,b,a)
scene.cube_wires(x,y,z, w,h,d, r,g,b,a)
scene.sphere(x,y,z, radius, r,g,b,a)
scene.sphere_wires(x,y,z, radius, rings, slices, r,g,b,a)
scene.plane(x,y,z, w,h, r,g,b,a)
scene.line3d(x1,y1,z1, x2,y2,z2, r,g,b,a)
scene.model_load(path)             -- int model id
scene.model_draw(id, x,y,z, scale, r,g,b,a)
scene.model_unload(id)
scene.tex_load(path)               -- int texture id
scene.tex_draw(id, x, y, r,g,b,a)
scene.tex_width(id)   scene.tex_height(id)
scene.tex_unload(id)
scene.fps_counter(x, y)
scene.time()                       -- number seconds since start
```

---

## Type casting
```lua
cast(int, 3.7)          -- 3      (truncate)
cast(number, my_int)    -- float
cast(uint8, 300)        -- 44     (wrapping truncation)
ptr_cast(ptr<int>, raw_ptr)
```

---

## sizeof / typeof
```lua
local sz: int = sizeof(Vec3)        -- bytes
local ts: string = typeof(my_val)   -- runtime type string (nonstrict)
```

---

## Compiler CLI
```
sluac file.slua                     -- compile to output.ll
sluac file.slua -o out.ll           -- compile to specific path
sluac file.slua --emit-ast          -- dump AST
sluac file.slua --emit-tokens       -- dump token stream
sluac file.slua --strict            -- force strict mode
sluac file.slua --nonstrict         -- force nonstrict mode
```

Compile + link:
```powershell
sluac examples\hello.slua -o bin\hello.ll
clang bin\hello.ll build\runtime\slua.lib raylib.lib -o hello.exe
.\hello.exe
```

Or use the PowerShell runner:
```powershell
.\slua.ps1 Slua-Run examples\hello.slua
```

---

## Error codes

| Code | Meaning |
|------|---------|
| E0001 | Parse error / unexpected token |
| E0010 | Redeclaration in strict mode |
| E0011 | Assignment to const |
| E0012 | Undefined identifier |
| E0013 | Use of uninitialised variable |
| E0020 | No type and no initialiser |
| E0030 | Void function returns value |
| E0031 | Missing return value |
| E0041 | for range not numeric |
| E0050 | store target not a pointer |
| E0051 | free requires pointer |
| E0052 | deref requires pointer |
| E0060 | Arithmetic on null |
| E0070 | Call on non-function |
| E0071 | Wrong argument count |
| E0080 | Field not found on record |
| E0090 | Null assigned to non-nullable |
| E0092 | Type mismatch |
| W0001 | Missing mode directive |
| W0012 | Undefined identifier (nonstrict) |

---

## Known Issues and Recent Fixes

### Recent Bug Fixes (April 8, 2026)

✅ **Fixed: For Loop Semantics** 
- Numeric for loops now use exclusive end condition (as documented)
- `for i = 0, 9 do` iterates 0→8 (not 0→9)
- Uses strict less-than comparison instead of less-than-or-equal

✅ **Fixed: Math Constants as Functions**
- `math.pi()` and `math.e()` are now proper function calls
- Return IEEE 754 double precision constants
- Both still work as expressions: `local x = math.pi()`

### Known Limitations

⚠️ **Optional Types Not Fully Implemented**
- Optional types (`int?`) are treated as `any` type in strict mode
- Cannot assign `null` to strict mode optional variables
- Workaround: Use nonstrict mode or use plain pointers (`ptr<...>`)

⚠️ **No For-In Loops**
- Iterator syntax `for k,v in pairs(table)` not supported
- Use C-style for loops or while loops instead

⚠️ **No Variadic Functions**
- Cannot use `...` parameter in function definitions
- Design restriction for static type system

⚠️ **String Memory Management**
- String operations allocate but don't auto-free
- No garbage collection; manual management required
- Use with caution in long-running loops

---

## Complete example
```lua
--!!type:strict
--!!mem:man
import math
import fs
import json
import http

type Config = { host: string, port: int, debug: int }

function Config.load(path: string): Config
    local data: string = fs.read_all(path)
    if data == "" then
        return { host = "localhost", port = 8080, debug = 0 }
    end
    return {
        host  = json.get_str(data, "host"),
        port  = json.get_int(data, "port"),
        debug = json.get_bool(data, "debug")
    }
end

function Config.to_url(self: Config): string
    return "http://" .. self.host .. ":" .. self.port
end

function main(): int
    local cfg: Config = Config.load("config.json")
    local url: string = Config.to_url(cfg)

    if cfg.debug == 1 then
        print("connecting to: " .. url)
    end

    local resp: string = http.get(url .. "/health")
    print("status: " .. resp)

    const N: int = 1024
    local buf: ptr<number> = alloc_typed(number, N)
    if buf == null then panic("alloc failed") end
    defer free(buf)

    for i = 0, N - 1, 1 do
        store(buf + i, math.sin(cast(number, i) * 0.01))
    end

    local sum: number = 0.0
    for i = 0, N - 1, 1 do
        sum = sum + deref(buf + i)
    end
    print("sum: " .. sum)

    return 0
end
```