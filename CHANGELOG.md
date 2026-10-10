# ART, Language Reference

Every feature in the language, with a one-line definition and
up to three examples where the shape isn't obvious. This is the
authoritative answer to "does ART have X".

Maintained by hand. If something here disagrees with the source,
the source wins, but fix the doc.

## Contents

**Values**
- [Variables](#variables)
- [Numbers](#numbers)
- [Strings](#strings)
- [Tables](#tables)

**Control flow**
- [if](#if)
- [while](#while)
- [repeat](#repeat)
- [for-range](#for-range)
- [for-in](#for-in)
- [break / continue / return](#break--continue--return)
- [switch](#switch)

**Functions**
- [Functions and closures](#functions-and-closures)
- [Lambdas](#lambdas)
- [Defaults and variadics](#defaults-and-variadics)

**Object orientation**
- [Classes](#classes)
- [Getters and setters](#getters-and-setters)
- [Operator overloading](#operator-overloading)
- [Static members](#static-members)
- [Inheritance and super](#inheritance-and-super)
- [Interfaces](#interfaces)
- [Enums](#enums)

**System**
- [Errors](#errors)
- [Imports](#imports)
- [Builtins](#builtins)
- [Patterns](#patterns)
- [Type checks](#type-checks)
- [Membership](#membership)

**Syntax details**
- [Multiline expressions](#multiline-expressions)
- [Keyword member names](#keyword-member-names)

## Variables

Three kinds. `local x` scopes to the current block; bare `x`
assigns a global (creating one if it doesn't exist); `const x`
marks a name immutable.

```art
local hp = 100           // block-scoped
score = 0                // global
const MAX_HP = 999       // immutable
```

`local` without a value declares with `nil`:

```art
local result
if (ready) { result = compute() }
```

Multi-assignment unpacks a table's array part. Missing slots
become `nil`, extra slots are ignored.

```art
local a, b, c = [1, 2, 3]
a, b = [10, 20]          // reassigns both, doesn't redeclare
local x, y = f()          // unpack a function's return
```

## Numbers

Two types: `int` (64-bit signed) and `float` (64-bit double).
Ints stay ints when both operands are ints; any float promotes.
`/` is **floor division** for ints.

```art
print(10 / 3)            // 3
print(10.0 / 3)          // 3.33333
print(10 % 3)            // 1, and negative-safe: -7 % 3 is 2
```

Literals: `42`, `1.5`, `1e10`, `1.5e-3`.

## Strings

UTF-16 internally, codepoint-indexed at the language level.
Immutable, every operation returns a new string. All strings
are interned, so `==` is pointer comparison.

### Literals

```art
local a = "hello"
local b = "line one\nline two"
local c = """raw, no escapes, spans
multiple lines"""
```

Triple-quoted strings take no escapes, backslashes are literal.

### Interpolation

`${expr}` evaluates an expression and inserts its `tostring`.

```art
local name = "Kara"
print("hello ${name}")            // hello Kara
print("2 + 2 = ${2 + 2}")         // 2 + 2 = 4
print("nested: ${list.map(fun(x) { return x * 2 })}")
```

### Format specs

`${expr:spec}` controls the rendering. Python-style grammar.

```art
local hp = 42
local pct = 0.755

"${hp:05d}"           // "00042"  , zero-padded to width 5
"${hp:#x}"            // "0x2a"   , hex with prefix
"${pct:.1f}%"         // "75.5%"  , one decimal
"${name:<10}|"        // "Kara      |", left-align
"${name:*^10}"        // "***Kara***", centered, star fill
```

Type letters: `d i x X o f e g s c`. Width, fill, alignment,
sign, `#` prefix, and `.precision` all supported.

### Methods

```art
"hello".length()              // 5
"hello".upper()               // "HELLO"
"hello".charAt(1)             // "h"     (1-based)
"hello".substring(2, 4)       // "el"    (1-based, end-exclusive)
"  hi  ".trim()               // "hi"
"42".toInt()                  // 42      (nil if not numeric)
"3.14".toFloat()              // 3.14
"hello".contains("ell")       // true
"hello".indexOf("l")          // 3
"hello".replace("l", "L")     // "heLLo"
"a-b-c".split("-")            // ["a", "b", "c"]
"a=b=c".split("=", 1)         // ["a", "b=c"]
"abc".reverse()               // "cba"
```

### Coercion

`+` with a string operand coerces the other side via `tostring`.
`*` with a string and an int repeats.

```art
"n = " + 5              // "n = 5"
5 + " items"            // "5 items"
"ab" * 3                // "ababab"
3 * "x"                 // "xxx"
"-" * 40                // 40 dashes, handy for dividers
```

`+` is the only operator that coerces. `"a" - 1` is a runtime
error, and so is `"5" * 2`, the string is a string, not a
number, so repeat doesn't apply (2 is fine, but 2.5 is not).

## Tables

The one collection type. Every table has a dense array part
(1-indexed) and/or a string-keyed hash part. Literals can mix.

```art
local arr = [1, 2, 3]
local map = ["hp" = 30, "mp" = 12]
local mixed = [10, 20, "name" = "Kara"]
```

### Access

```art
arr[1]                  // 1        (1-based)
arr[-1]                 // 3        (negative counts from end)
map["hp"]               // 30
map.hp                  // 30       (dot syntax for string keys)
mixed.name              // "Kara"
```

Out-of-range reads return `nil`. Negative-index writes that
would land before 1 raise an error.

### Rendering

`tostring` and `print` render tables structurally:

```art
tostring([1, 2, 3])                 // "[1, 2, 3]"
tostring(["a" = 1, "b" = 2])        // "{"a": 1, "b": 2}"
tostring([[1,2],[3,4]])             // "[[1, 2], [3, 4]]"
"got " + [1, 2, 3]                  // "got [1, 2, 3]"
```

Strings inside a rendered table are quoted, so `["a", "b"]` is
distinguishable from `[a, b]`. Cyclic tables render `[...]` past
depth 4. Long output truncates with `, ...` past 512 chars.

### Methods

Array operations:

```art
local t = [1, 2, 3]

t.push(4, 5, 6)         // append several at once
t.pop()                 // 6, removes from end
t.length()              // 5
t.isEmpty()             // false
t.contains(3)           // true, searches values, both halves
t.indexOf(3)            // 3
t.insert(2, 99)         // insert at position 2
t.remove(1)             // remove and return value at position 1
t.reverse()             // in place
t.clear()               // empties both array and hash
t.slice(2, 4)           // new table, elements 2..4 inclusive
t.clone()               // shallow copy of both halves
t.join(", ")            // "1, 2, 3", tostring each, join with sep
```

Higher-order operations (array part only):

```art
[1, 2, 3].map(fun(x) { return x * 2 })     // [2, 4, 6]
[1, 2, 3, 4].filter(fun(x) { return x % 2 == 0 })   // [2, 4]
[1, 2, 3].reduce(fun(acc, x) { return acc + x }, 0) // 6
[1, 2, 3].forEach(fun(x) { print(x) })
[1, 2, 3].any(fun(x) { return x > 2 })     // true
[1, 2, 3].all(fun(x) { return x > 0 })     // true
[1, 2, 3].find(fun(x) { return x > 1 })    // 2
[1, 2, 3, 4].count(fun(x) { return x > 2 }) // 2
```

Sorting:

```art
local t = [3, 1, 4, 1, 5]
t.sort()                                    // ascending
t.sort(fun(a, b) { return a > b })          // descending, custom
```

Keys and values:

```art
["a" = 1, "b" = 2].keys()       // ["a", "b"], order unspecified
["a" = 1, "b" = 2].values()     // [1, 2]
```

### Iteration

`for-in` walks the array part first (keys 1..n), then the hash
part (string keys). Order across the hash part is unspecified.

```art
for (local v in [10, 20, 30]) {
    print(v)                     // 10, 20, 30
}

for (local k, v in ["a" = 1, "b" = 2]) {
    print(k + "=" + v)           // a=1, b=2 (order may vary)
}
```

## if

Two forms. Statement form uses blocks:

```art
if (hp > 0) {
    print("alive")
} else if (hp == 0) {
    print("dead")
} else {
    print("what")
}
```

Expression form uses `->`. Returns the branch value.

```art
local status = if (hp > 0) -> "alive" else "dead"
local mult = if (defending) -> 0.5 else 1.0
```

`else` is optional in expression form; missing branch yields
`nil`.

```art
local cached = if (k in cache) -> cache[k]
```

The expression form is preferred over the `and/or` trick
because it doesn't break on falsy values:

```art
local x = (user and user.name) or "guest"    // BROKEN if name is nil
local x = if (user) -> user.name else "guest"  // correct
```

## while

```art
local i = 0
while (i < 10) {
    print(i)
    i = i + 1
}
```

## repeat

```art
local n = 0
repeat {
    n = n + 1
} until (n >= 5)
n                                // 5
```

The body always runs once, unlike `while`, which checks
before the first iteration:

```art
local n = 0
repeat {
    n = n + 1
} until (true)
n                                // 1
```

`break` and `continue` work as in `while`. `continue` jumps
to the condition check — the sensible interpretation, since
"jump to the top of the body" would infinitely loop on the
first iteration.

```art
local tries = 0
repeat {
    tries = tries + 1
    if (tries < 3) { continue }
} until (tries >= 3)
tries                            // 3
```

## for-range

Inclusive on both ends. Optional step, defaults to 1.

```art
for (local i = 1 -> 10) {
    print(i)                     // 1, 2, 3, ..., 10
}

for (local i = 10 -> 1; -1) {
    print(i)                     // 10, 9, ..., 1
}

for (local i = 0 -> 100; 5) {
    print(i)                     // 0, 5, 10, ..., 100
}
```

The loop variable is fresh per iteration, so closures capture
the value at their own iteration:

```art
local fns = []
for (local i = 1 -> 3) {
    fns.push(fun() { return i })
}
fns[1]() + fns[2]() + fns[3]()   // 6 (not 12)
```

## for-in

See Tables -> Iteration.

## break / continue / return

Standard. `break` and `continue` only inside loops. `return`
only inside functions (top-level `return` ends the program).

```art
for (local i = 1 -> 100) {
    if (i == 3) { break }
    if (i % 2 == 0) { continue }
    print(i)                     // 1
}
```

## switch

Expression or statement. No fall-through. Arms use `->`.

```art
local name = switch (code) {
    200 -> "OK"
    404 -> "Not Found"
    500 -> "Server Error"
    else -> "Unknown"
}
```

Multiple values per arm, comma-separated:

```art
local kind = switch (n) {
    1, 3, 5, 7, 9 -> "odd"
    0, 2, 4, 6, 8 -> "even"
    else -> "?"
}
```

Type-checked. Every case value must have the same type as the
subject, or a runtime error fires.

```art
switch (color) {
    Color.Red -> "red"           // enum values work
    Color.Green, Color.Blue -> "cool"
    else -> "?"
}
```



## Functions and closures

First-class. Closures capture the scope they were defined in.

```art
fun add(a, b) {
    return a + b
}

local result = add(3, 4)         // 7
```

Closures capture variables by reference:

```art
fun makeCounter() {
    local n = 0
    return fun() {
        n = n + 1
        return n
    }
}

local c = makeCounter()
c()                              // 1
c()                              // 2
```

Recursion works, but the function must be in scope:

```art
fun fib(n) {
    if (n < 2) { return n }
    return fib(n - 1) + fib(n - 2)
}
fib(10)                          // 55
```



## Lambdas

Two syntaxes. `x -> expr` for a single param, `(a, b) -> expr`
for multiple. Body can be an expression or a block.

```art
local double = x -> x * 2
local add = (a, b) -> a + b
local noop = () -> nil

[1, 2, 3].map(x -> x * 2)        // [2, 4, 6]
```

Multi-line bodies use braces:

```art
local classify = fun(x) {
    if (x > 0) { return "positive" }
    if (x < 0) { return "negative" }
    return "zero"
}
```



## Defaults and variadics

Trailing params can have defaults. `...rest` collects extra
arguments into a table.

```art
fun greet(name, greeting = "hello") {
    return greeting + ", " + name
}

greet("Kara")                    // "hello, Kara"
greet("Kara", "hi")              // "hi, Kara"
```

Variadic:

```art
fun sum(...args) {
    return args.reduce(fun(a, b) { return a + b }, 0)
}
sum(1, 2, 3, 4)                  // 10
sum()                            // 0
```

Mixed fixed and variadic:

```art
fun log(level, ...parts) {
    print("[" + level + "] " + parts.join(" "))
}
log("INFO", "user", "logged", "in")
```



## Classes

Fields must be declared at the top of the class body before any
method assigns to them. `local` makes a field private to the
class. Public is the default.

```art
class Point {
    x = 0
    y = 0

    fun Point(x, y) {
        this.x = x
        this.y = y
    }

    fun sum() {
        return this.x + this.y
    }
}

local p = Point(3, 4)
p.x                              // 3
p.sum()                          // 7
```

The constructor is the method whose name matches the class.

```art
class Box {
    v = 0
    fun Box(v) { this.v = v }    // constructor
}
Box(42).v                        // 42
```

Bare names inside a method resolve in this order:

1. Locals and parameters of the method
2. Enclosing scopes (the closure's captured scope chain)
3. **Class members via `this`** , fields, then getters, then
methods
4. Global scope
5. `nil`

That means a local shadows a field, and a field can be
reached without the `this.` prefix. Same resolution order as
Java and C++.

```art
class C {
    name = "field"
    fun C() { }
    fun fetch() {
        return name              // "field": implicit this.name
    }
    fun shadow() {
        local name = "local"
        return name              // "local": local shadows field
    }
    fun double() {
        return this.fetch() + fetch()  // both work
    }
}
```

`this.` is still the explicit form and still works. The
implicit fallback only fires when the name didn't resolve in
the scope chain, so a helper function named `fetch` in an
outer scope takes precedence over a method named `fetch` on
the receiver.

Private members with `local`. The rule applies to fields,
methods, and getters alike:

```art
class Counter {
    local count = 0
    local fun internalReset() { this.count = 0 }

    fun Counter() { }
    fun bump() { this.count = this.count + 1 }
    get value() { return this.count }
}

local c = Counter()
c.bump()
c.value                          // 1

c.count                          // ERROR: field is private
c.internalReset()                // ERROR: method is private
```

`local fun` on a method makes it private, callable from
within the class, invisible from outside. Same privacy
checks the field path uses.

Reflection respects privacy too: `c.fields()` does not list
`count` or `internalReset`, and `c.get("count")` from
outside the class errors.

### Reflection

Instances expose three methods for walking their members
dynamically.

`obj.fields()` returns an array of the accessible member
names — fields, getters, and methods from this class and
every superclass. Private members (declared with `local`)
and static members are excluded. Duplicates across the
inheritance chain are collapsed to one entry.

```art
class Entity {
    name = "goblin"
    hp = 30
    fun Entity() { }
    fun takeDamage(n) { this.hp = this.hp - n }
    local fun internalTick() { return 0 }
}

local e = Entity()
e.fields()             // ["name", "hp", "takeDamage"] — any order
```

`obj.get(name)` reads a member by name. Fields return their
value, getters are invoked, methods return a bound method
you can call.

```art
e.get("name")                    // "goblin"
e.get("hp")                      // 30
e.get("takeDamage")(5)           // calls the method
e.get("hp")                      // 25
```

`obj.set(name, value)` writes a field or invokes a setter.

```art
e.set("name", "orc")
e.set("hp", 50)
e.name                           // "orc"
```

Private members are not accessible through reflection from
outside the class. Inside a method, the class's own private
members can be reached via `this.get("...")` and
`this.set("...", ...)`.

A user-defined `fun fields()`, `fun get()`, or `fun set()`
on a class overrides the built-in for that class.

Reflection is what makes recursive serialization possible:

```art
fun dump(obj, indent) {
    local pad = " " * indent
    for (local name in obj.fields()) {
        local v = obj.get(name)
        if (v is Entity) {
            print(pad + name + ":")
            dump(v, indent + 2)
        } else {
            print(pad + name + " = " + tostring(v))
        }
    }
}
```

## Getters and setters

Declared with `get name {}` and `set name(v) {}`. Called by
ordinary member access, no parens at the call site.

```art
class Temperature {
    local _celsius = 0

    fun Temperature(c) { this._celsius = c }

    get celsius() { return this._celsius }
    set celsius(v) { this._celsius = v }

    get fahrenheit() { return this._celsius * 9 / 5 + 32 }
}

local t = Temperature(100)
t.fahrenheit                     // 212
t.celsius = 0
t.fahrenheit                     // 32
```



## Operator overloading

Any of `+ - * / % ^ < > <= >= == !=` plus unary `-`. Params can
be typed, and multiple overloads can share a name.

```art
class Vec {
    x = 0
    y = 0

    fun Vec(x, y) { this.x = x; this.y = y }

    operator +(o: Vec) {
        return Vec(this.x + o.x, this.y + o.y)
    }

    operator *(s: Number) {
        return Vec(this.x * s, this.y * s)
    }
}

local v = Vec(1, 2) + Vec(3, 4)  // Vec(4, 6)
local s = Vec(1, 2) * 3          // Vec(3, 6)
```

`==` defines equality; `!=` is derived from it.

```art
class Vec {
    x = 0
    fun Vec(x) { this.x = x }
    operator ==(o) { return this.x == o.x }
}

Vec(1) == Vec(1)                 // true
Vec(1) != Vec(2)                 // true, no operator != needed
```

Commutative operators try the right operand if the left doesn't
have a matching overload. This is what lets `2 * v` work:

```art
class Vec {
    x = 0
    fun Vec(x) { this.x = x }
    operator *(s: Number) { return Vec(this.x * s) }
}

(2 * Vec(5)).x                   // 10
```

Non-commutative ops (`-`, `/`) don't reverse. `10 - Vec(5)`
errors unless Vec has an `operator -` accepting a number.



## Static members

`static fun name()` for methods, `static field = x` for fields.
Both are accessed through the class, not an instance.

```art
class Counter {
    static total = 0

    fun Counter() {
        Counter.total = Counter.total + 1
    }

    static fun reset() {
        Counter.total = 0
    }
}

Counter()
Counter()
Counter.total                    // 2
Counter.reset()
Counter.total                    // 0
```



## Inheritance and super

`extends` for a superclass. `super(...)` calls the parent's
constructor; `super.method()` calls a parent method.

```art
class Animal {
    name = ""
    fun Animal(name) { this.name = name }
    fun speak() { return "..." }
}

class Dog extends Animal {
    fun Dog(name) { super(name) }
    fun speak() { return "woof" }
    fun parentSpeak() { return super.speak() }
}

local d = Dog("Rex")
d.name                           // "Rex"
d.speak()                        // "woof"
d.parentSpeak()                  // "..."
```



## Interfaces

A contract. Declarations only, no bodies. Verification runs
when the class is defined, not when a method is called.

```art
interface Drawable {
    fun draw()
    get bounds
}

class Circle implements Drawable {
    r = 0
    fun Circle(r) { this.r = r }
    fun draw() { print("circle") }
    get bounds { return this.r * 2 }
}
```

Inheritance between interfaces:

```art
interface Animal { fun breathe() }
interface Pet extends Animal { fun name() }

class Dog implements Pet {
    fun Dog() { }
    fun breathe() { }
    fun name() { return "Rex" }
}
Dog() is Animal                   // true
```

Variadic method requirements:

```art
interface Logger {
    fun log(level, ...parts)
}

class ConsoleLogger implements Logger {
    fun ConsoleLogger() { }
    fun log(level, ...parts) {
        print("[" + level + "] " + parts.join(" "))
    }
}
```

The class must accept every arity the interface accepts. A fixed
`fun log(a)` doesn't satisfy `fun log(...parts)`.



## Enums

Named singletons with optional values.

```art
enum Color { Red, Green, Blue }
enum Http { OK = 200, NotFound = 404 }
```

Reading members and values:

```art
Color.Red                        // the enum value
Http.NotFound.value              // 404
Color.Red.name                   // "Red"
tostring(Color.Red)              // "Color.Red"
```

Helpers on the enum itself:

```art
Color.values()                   // [Red, Green, Blue]
Color.names()                    // ["Red", "Green", "Blue"]
Color.fromName("Green")          // Color.Green, or nil
Http.fromValue(404)              // Http.NotFound, or nil
```

Membership check with `in`:

```art
Color.Red in Color               // true
Other.Red in Color               // false
```



## Errors

Runtime errors unwind to the nearest error boundary and print
`file:line:col: error: message` with a stack trace.

### Raising

```art
error("something went wrong")
```

The argument can be any value: a string, a table, an
instance, a number. The value is stored as-is and handed
back by `attempt()` unchanged. This is what lets a game
engine throw structured data:

```art
error(["code" = 404, "reason" = "not found"])
error(500)
error(SomeCustomError("connection refused"))
```

Interpreter-level errors (division by zero, undefined
member, wrong argument count) produce a formatted string as
the payload, a message with file, line, column, and the
reason.

###  Catching

`attempt(fn, ...)` returns `[ok, payload]`. On success,
payload is the return value. On failure, payload is whatever
was thrown.

```art
local ok, result = attempt(fun() { return compute() })
if (ok) {
    print("got " + result)
} else {
    print("failed: " + result)
}
```

Structured throws come back structured:

```art
local ok, info = attempt(fun() {
    error(["code" = 404, "reason" = "not found"])
})
if (!ok) {
    print("code " + info.code + ": " + info.reason)
}
```

Interpreter errors come back as strings:

```art
local ok, msg = attempt(fun() { 1 / 0 })
// ok is false, msg is "«file»:«line»:«col»: error: division by zero"
```
### Asserting

`assert(cond)` raises if `cond` is falsy. Optional message:

```art
assert(entity.hp > 0)
assert(n >= 0, "n must be non-negative")
assert(items.length() > 0, "cannot process empty list")
```



## Imports

`import "path"` loads and evaluates a file. The module's value is
its top-level `return` (or `nil`). Cached by absolute path.

```art
local Math2 = import "stdlib/math_extra.art"
local JSON = import "stdlib/json.art"
```

Paths are relative to the importing file. `.art` extension is
optional.

```art
import "utils"                   // looks for ./utils.art
import "lib/helpers.art"         // explicit
```

Cycles are detected and raise an error.



## Builtins

Global functions always available:

```art
print(a, b, ...)                 // space-separated, newline after
tostring(v)                      // string form of any value
error(msg)                       // raise
assert(cond [, msg])             // raise if falsy
attempt(fn, ...)                 // catch runtime errors
import "path"                    // module loader
```

### Math

```art
Math.pi, Math.tau, Math.e, Math.inf, Math.nan

Math.abs(x)        Math.floor(x)      Math.ceil(x)
Math.round(x)      Math.trunc(x)      Math.sqrt(x)
Math.pow(x, y)     Math.exp(x)        Math.log(x)
Math.log2(x)       Math.log10(x)
Math.sin(x)        Math.cos(x)        Math.tan(x)
Math.asin(x)       Math.acos(x)       Math.atan(x)
Math.atan2(y, x)
Math.min(a, b)     Math.max(a, b)     Math.clamp(v, lo, hi)
Math.sign(x)       Math.deg(x)        Math.rad(x)
Math.random([n])   Math.random(lo, hi)
Math.seed(n)                     // deterministic
```

### File

```art
File.read(path)                  // string, or nil on missing
File.write(path, contents)
File.append(path, contents)
File.delete(path)

local f = File.open(path)
f.read()                         // whole file
f.readLine()                     // one line, or nil at EOF
f.readLines()                    // table of lines
f.write(contents)
f.append(contents)
f.close()
```

### Time

```art
Time.now()                       // float seconds since epoch
Time.clock()                     // CPU seconds, monotonic
Time.sleep(ms)                   // blocks
```



## Patterns

Lua-style pattern matching. NOT regex.

Syntax:

- Classes: `%a` letter, `%d` digit, `%w` alphanumeric, `%s`
space, `%l` lowercase, `%u` uppercase, `%p` punctuation, `%x`
hex digit. Uppercase is the complement.
- Sets: `[abc]`, `[^abc]`, `[a-z]`, `[%d ]`.
- Quantifiers: `*` (0+ greedy), `+` (1+ greedy), `-` (0+ lazy),
`?` (0 or 1).
- Anchors: `^` start, `$` end.
- `.` any char.

### find

Returns `[start, end, cap1, ...]` (1-based inclusive), or `nil`.

```art
"hello world".find("world")       // [7, 11]
"abc123".find("%d+")              // [4, 6]
"abc".find("xyz")                 // nil
"key=val".find("(%a+)=(%a+)")     // [1, 7, "key", "val"]
```

### match

Returns the whole match, the single capture, or a table of
captures.

```art
"abc123".match("%d+")             // "123"
"key=val".match("(%a+)=(%a+)")    // ["key", "val"]
"key=val".match("(%a+)=")         // "key"
"abc".match("%d+")                // nil
```

### gmatch

Returns a table of every match.

```art
"foo bar baz".gmatch("%a+")       // ["foo", "bar", "baz"]
"a=1,b=2".gmatch("(%a+)=(%d+)")   // [["a", "1"], ["b", "2"]]
```

### gsub

Replace all matches. Replacement can be a string with `%0`
(whole match), `%1..%9` (captures), `%%` (literal), or a
function called with the captures.

```art
"a-b-c".gsub("-", "+")                       // "a+b+c"
"a=1,b=2".gsub("(%a+)=(%d+)", "%2:%1")       // "1:a,2:b"
"cat".gsub("a", "[%0]")                      // "c[a]t"
"a1b2".gsub("%d", fun(d) { return "<" + d + ">" })  // "a<1>b<2>"
```

Limit replacements with a third argument:

```art
"a-b-c-d".gsub("-", "+", 2)                  // "a+b+c-d"
```



## Type checks

`is` for both builtin types and user classes/interfaces.

```art
5 is Int                         // true
5.0 is Int                       // false
5 is Number                      // true
"hi" is String                   // true
[1] is Table                     // true
nil is Nil                       // true

class V { fun V() { } }
V() is V                         // true
V() is SomeInterface             // interface check
```

Builtin type names: `Any`, `Number`, `Int`, `Float`, `String`,
`Bool`, `Nil`, `Table`, `Function`.



## Membership

`in` works on tables, strings, and enums.

```art
2 in [1, 2, 3]                   // true, array value
"x" in ["x" = 1]                 // true, hash key
"xyz" in "hello"                 // false, substring
"ell" in "hello"                 // true
Color.Red in Color               // true
```

Table `in` checks both the array part (values) and the hash part
(keys). It doesn't check hash values, use `.contains()` for
that.



## Multiline expressions

A binary operator or `.` at the start of a line continues the
previous expression. Anything else starts a new statement.

```art
local total = 1 +
    2 +
    3
// total is 6

local names = people
    .filter(fun(p) { return p.age > 18 })
    .map(fun(p) { return p.name })
    .join(", ")
```



## Keyword member names

After `.`, a keyword can be used as a member name. This lets
methods be called `get`, `set`, `class`, `static`, etc.

```art
class A {
    fun A() { }
    fun get() { return 1 }
    fun static() { return 2 }
}

A().get()                        // 1
A().static()                     // 2
["get" = 5].get                  // 5
```



## Not in ART

Things that were considered and rejected, or removed:

- **`String.format()`**, removed. Use interpolation specs
(`"${x:05d}"`). One formatting mechanism, not two.
- **`String.repeat()`**, removed. Use the `*` operator
(`"ab" * 3`).
- **Regex**, not implemented. Lua patterns cover the common
cases.
- **Coroutines**, not implemented. Requires platform-specific
stack switching.
- **Nil-safe operators (`?.`, `??`)**, not implemented.
`attempt` and `assert` cover the error-handling need.
- **Named arguments**, not implemented. Positional only.
- **Multiple return values**, not implemented. Return a table
and use multi-assignment to unpack it.
- **Range operator (`..`)**, not implemented. `for-range` uses
`->`, table slicing uses `.slice(a, b)`.

## Known limitations

Things that work but have caveats. Not bugs — decisions that
trade simplicity for something else, and the cost is worth
documenting so it isn't rediscovered as a surprise.

### Intern table growth

Every string ART creates goes into `S->strings`, and that table
is a GC root. So strings are never freed. A loop that produces
N unique strings leaves N permanent entries.

    for (local i = 1 -> 10000) {
        local msg = "tick " + i
        log(msg)          // "tick 1", "tick 2", ... all retained
    }

Long-running processes that generate unique strings — log lines,
save-file paths, formatted timestamps — will grow memory
unboundedly. Games running for hours will notice.

This is a design consequence of interning everything, and the
alternative costs more than it saves right now. Fixing it means
either weak references in the intern table (needs a new GC pass
to prune dead entries) or splitting strings into interned vs.
transient classes (needs a new variant of every string API).

**Target for: the VM rewrite.** String constant ownership
changes there anyway.

### Recursion limits

Two hard caps, both to prevent stack overflow:

- `ART_FRAMES_MAX` (2048) — nesting depth of function calls.
  A too-deep recursion raises `call depth limit reached`
  rather than crashing.
- `PARSER_MAX_DEPTH` (500) — expression nesting depth in the
  parser. `(((((...)))))` past 500 raises
  `expression nesting too deep` rather than blowing the C
  stack.

Both are generous for hand-written code and small for
adversarial input. Neither is adjustable without recompiling.

### Rendering caps

Table rendering truncates with `, ...` past 512 characters of
output, and stops descending at depth 4 for cyclic structures.
Both are to keep `print(giantTable)` from printing megabytes
or looping forever, but they mean `tostring(hugeTable)` isn't
lossless.

If you need the full structure, walk it manually with `keys()`
and `values()`.

### No sandboxing

`File.read`, `File.write`, and friends are unconditionally
available to any script. If your engine loads untrusted
scripts — user-authored mods, downloaded maps — you either
strip the `file` and `time` features before building, or you
don't run untrusted code.

There's no runtime flag to disable them. The feature registry
exists (`FEATURES(X)` in `features.h`), but nothing exposes a
"which features to install" knob through `art_open`. That
would be a small addition if anyone needs it.

### No async, no coroutines

Scripts run to completion. There's no way to write "wait 3
seconds, then do this" as linear code. The engine either
schedules the resume itself (state machine in the script) or
the script blocks the frame (`Time.sleep`).

This is a real gap for game engines. It's also the reason
several things in ART are simpler than they'd otherwise be —
the whole runtime assumes a single-threaded, synchronous call
stack.