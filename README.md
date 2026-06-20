<div align="center">
  <img src="Velo.svg" alt="Velo" width="120" height="120">

# Velo

**A modern scripting language built on LuaJIT**

Fast as LuaJIT. Clean modern syntax. JIT-compiled or compiled to native executables.

[![License](https://img.shields.io/badge/license-Apache%202.0-922992)](LICENSE)
[![Made in Poland](https://img.shields.io/badge/made%20in-Poland-922992)](https://github.com/Nexoniarz/velo)

</div>

---

## Quick Start

```bash
# Build from source
make -j$(nproc)
cd src && ./velo hello.velo
```

```velo
// hello.velo
print("Hello from Velo!")
```

---

## Learn Velo in 5 Minutes

### Comments

```velo
// This is a comment
```

> Note: `--` is the **decrement operator** in Velo, not a comment.

---

### Variables

```velo
local x = 10
local name = "Velo"
local ok = true
local nothing = nil
```

---

### Arithmetic & Assignment

```velo
local x = 5
x += 3    // x = 8
x -= 1    // x = 7
x *= 2    // x = 14
x /= 2    // x = 7
x++       // x = 8
x--       // x = 7

local y = x + 1
local z = x % 3    // modulo
local p = x ^ 2    // power
```

---

### Strings

```velo
local s = "Hello, " .. "world!"    // concatenation
local n = #s                        // length
local upper = string.upper(s)
local sub = string.sub(s, 1, 5)
```

---

### Comparisons & Logic

```velo
x == y     // equal
x != y     // not equal  (Velo: != instead of ~=)
x < y
x > y
x <= y
x >= y

x > 0 and x < 10
x < 0 or x > 100
not true
```

---

### Conditionals

Blocks use `{` `}` instead of `then`/`end`:

```velo
if x > 10 {
    print("big")
} else if x == 10 {
    print("ten")
} else {
    print("small")
}
```

> Note: `else if` must be on the same line as `}`.

---

### Loops

```velo
// Numeric for
for i = 1, 10 {
    print(i)
}

// Numeric for with step
for i = 0, 100, 5 {
    print(i)
}

// Generic for (iterators)
local t = {10, 20, 30}
for i, v in ipairs(t) {
    print(i, v)
}

for k, v in pairs(someTable) {
    print(k, v)
}

// While loop
local i = 0
while i < 10 {
    i++
}

// Repeat-until
repeat
    i--
until i == 0
```

---

### Functions

```velo
function greet(name) {
    return "Hello, " .. name .. "!"
}

print(greet("Velo"))

// Multiple return values
function minmax(a, b) {
    if a < b {
        return a, b
    }
    return b, a
}

local lo, hi = minmax(5, 3)

// Varargs
function sum(...) {
    local total = 0
    local args = {...}
    for i = 1, #args {
        total += args[i]
    }
    return total
}
```

---

### Tables

Tables are the only data structure. They work as arrays and dictionaries.

```velo
// Array-style (0-based constructor indexing in Velo)
local arr = {10, 20, 30}
// arr[0] = 10, arr[1] = 20, arr[2] = 30

// Dict-style
local person = {
    name = "Alice",
    age = 30,
}
print(person.name)
print(person["age"])

// Mixed
local mixed = {
    x = 1,
    y = 2,
    100, 200,    // indices 0, 1
}

// Nested tables
local matrix = {
    {1, 2, 3},
    {4, 5, 6},
}
print(matrix[0][1])    // 2 (0-based)
```

---

### Object-Oriented Programming

Velo has built-in `class`, `new`, and `self`/`this` keywords:

```velo
class Animal {
    function Animal(name, sound) {
        self.name = name
        self.sound = sound
    }

    function speak() {
        print(self.name .. " says " .. self.sound)
    }

    function getName() {
        return self.name
    }
}

local dog = new Animal("Rex", "Woof")
dog.speak()               // Rex says Woof
print(dog.getName())      // Rex
```

**Inheritance** (manual delegation):

```velo
class Dog {
    function Dog(name) {
        self.name = name
        self.base = new Animal(name, "Woof")
    }

    function fetch(item) {
        print(self.name .. " fetches the " .. item)
    }

    function speak() {
        self.base.speak()
    }
}

local d = new Dog("Buddy")
d.fetch("ball")
d.speak()
```

---

### Modules

```velo
// mymodule.velo
local M = {}

function M.hello() {
    print("Hello from module!")
}

return M
```

```velo
// main.velo
local m = require("mymodule")
m.hello()
```

---

### Error Handling

```velo
local ok, err = pcall(function() {
    error("something went wrong")
})

if not ok {
    print("Error: " .. err)
}

// xpcall with traceback
local function handler(err)
    return debug.traceback(err, 2)
end

local ok, msg = xpcall(riskyFunction, handler)
```

---

### String Patterns

```velo
local s = "Hello, World!"

// Find
local start, finish = string.find(s, "World")

// Match
local word = string.match(s, "%a+")

// Global match
for word in string.gmatch(s, "%a+") {
    print(word)
}

// Replace
local result = string.gsub(s, "World", "Velo")
```

---

### FFI (C interop via LuaJIT)

```velo
local ffi = require("ffi")

ffi.cdef[[
    int printf(const char *fmt, ...);
    void *malloc(size_t size);
    void free(void *ptr);
]]

ffi.C.printf("Hello from C: %d\n", 42)

// Load shared library
local lib = ffi.load("libm.so.6")
ffi.cdef[[ double sqrt(double x); ]]
print(lib.sqrt(2.0))
```

---

## Full Keyword Reference

| Keyword    | Description                                      |
|------------|--------------------------------------------------|
| `and`      | Logical AND operator                             |
| `break`    | Exit the current loop                            |
| `class`    | Define an OOP class                              |
| `do`       | Begin a block (in `while`/`for` without `{}`)   |
| `else`     | Else branch of `if`                              |
| `elseif`   | Chained else-if (alternative to `else if`)      |
| `end`      | End of a block (compatibility mode)              |
| `false`    | Boolean false literal                            |
| `for`      | Numeric or generic for loop                      |
| `function` | Define a function                                |
| `goto`     | Jump to a label                                  |
| `if`       | Conditional                                      |
| `in`       | Used in generic `for ... in`                     |
| `local`    | Declare a local variable                         |
| `new`      | Instantiate a class                              |
| `nil`      | Null/empty value                                 |
| `not`      | Logical NOT operator                             |
| `or`       | Logical OR operator                              |
| `repeat`   | Begin a repeat-until loop                        |
| `return`   | Return from a function                           |
| `then`     | Used after `if` condition (compatibility)        |
| `this`     | Alias for `self` inside class methods            |
| `true`     | Boolean true literal                             |
| `until`    | End condition for `repeat`                       |
| `while`    | While loop                                       |

---

## Full Operator Reference

### Arithmetic

| Operator | Meaning             |
|----------|---------------------|
| `+`      | Addition            |
| `-`      | Subtraction         |
| `*`      | Multiplication      |
| `/`      | Division            |
| `%`      | Modulo              |
| `^`      | Power               |
| `-x`     | Unary negation      |

### Compound Assignment

| Operator | Meaning              |
|----------|----------------------|
| `+=`     | Add and assign       |
| `-=`     | Subtract and assign  |
| `*=`     | Multiply and assign  |
| `/=`     | Divide and assign    |
| `++`     | Increment by 1       |
| `--`     | Decrement by 1       |

### Comparison

| Operator | Meaning              |
|----------|----------------------|
| `==`     | Equal                |
| `!=`     | Not equal            |
| `<`      | Less than            |
| `>`      | Greater than         |
| `<=`     | Less or equal        |
| `>=`     | Greater or equal     |

### String & Misc

| Operator | Meaning                       |
|----------|-------------------------------|
| `..`     | String concatenation          |
| `...`    | Varargs                       |
| `#`      | Length of string/table        |
| `//`     | Line comment                  |
| `{ }`    | Block delimiters (if/for/etc) |

### Bitwise (via `bit` library)

```velo
local bit = require("bit")
bit.band(a, b)    // AND
bit.bor(a, b)     // OR
bit.bxor(a, b)    // XOR
bit.bnot(a)       // NOT
bit.lshift(a, n)  // left shift
bit.rshift(a, n)  // right shift
```

---

## CLI Reference

```
Usage: velo [options] [script [args]]

Options:
  -e chunk         Execute string as Velo code
  -l name          Require library 'name'
  -b ...           Save or list bytecode
  -j cmd           Perform JIT control command
  -O [opt]         Control JIT optimizations
  -i               Enter interactive mode after executing script
  -v               Show version information
  -E               Ignore environment variables
  --               Stop processing options

Velo extensions:
  --compile <src> [<out>]          Compile source to .veloc bytecode
  --compile --exec <src> [<out>]   Compile source to native executable
  --lua                            Run with JIT disabled (pure interpreter)
```

### Compile to bytecode

```bash
velo --compile hello.velo           # produces hello.veloc
velo --compile hello.velo out.veloc # explicit output name

# Run bytecode
velo hello.veloc
```

### Compile to native executable

```bash
velo --compile --exec hello.velo          # produces hello (ELF)
velo --compile --exec hello.velo myprog   # explicit output name

# Run the native executable (no velo required)
./hello
./myprog
```

Native executables are fully standalone — they embed the Velo runtime and your program's bytecode. No `velo` installation needed to run them.

### Pure interpreter mode

```bash
velo --lua script.velo    # JIT disabled, pure interpreter
```

Useful for debugging JIT-related issues or environments where JIT is not permitted.

### REPL

```bash
velo        # start interactive REPL
velo -i     # interactive mode after running a script
```

```
  Velo 1.0.0  |  Apache License 2.0
  Copyright (C) 2026 Nexoniarz. Based on LuaJIT © 2005-2026 Mike Pall.
  https://github.com/Nexoniarz/velo

velo> print("hello")
hello
velo> 
```

---

## Standard Library

Velo inherits all LuaJIT standard libraries:

| Library    | Prefix       | Description                                                        |
|------------|--------------|--------------------------------------------------------------------|
| Base       | (global)     | `print`, `type`, `pairs`, `ipairs`, `pcall`, `error`, `require`, ... |
| String     | `string.*`   | Pattern matching, format, sub, ...                                  |
| Table      | `table.*`    | `insert`, `remove`, `sort`, `concat`, ...                           |
| Math       | `math.*`     | `sin`, `cos`, `floor`, `ceil`, `sqrt`, `random`, ...               |
| IO         | `io.*`       | File I/O                                                           |
| OS         | `os.*`       | `time`, `clock`, `date`, `exit`, ...                               |
| Package    | `package.*`  | Module loading                                                     |
| Debug      | `debug.*`    | Introspection                                                      |
| Bit        | `bit.*`      | Bitwise operations                                                  |
| JIT        | `jit.*`      | JIT control                                                        |
| FFI        | `ffi.*`      | C interop (LuaJIT FFI)                                             |

---

## Building

```bash
git clone https://github.com/Nexoniarz/velo
cd velo
make -j$(nproc)
# binary at: src/velo
```

Requirements: GCC or Clang, GNU Make.

---

## License

Apache License 2.0 — Copyright (C) 2026 Nexoniarz.  
Based on [LuaJIT](https://luajit.org/) by Mike Pall (MIT License).
