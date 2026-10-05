# ART

A dynamically-typed scripting language.

## Running

    make
    ./bin/art script.art
    ./bin/art

On Windows, `bootstrap.bat` installs MSYS2 and the toolchain in
one step. On Linux and macOS, `./bootstrap.sh` does the same.
For build instructions on other platforms, IDEs, and MSVC, see
`BUILDING.md`.

## Syntax

    local x = 10
    y = 20
    const MAX = 100

    fun add(a, b) { return a + b }

    fun makeCounter() {
        local n = 0
        return fun() { n = n + 1
    return n }
    }

    class Point {
        x = 0
        y = 0

        fun Point(x, y) {
            this.x = x
            this.y = y
        }

        operator +(o: Point) {
            return Point(this.x + o.x, this.y + o.y)
        }

        get magnitude() {
            return Math.sqrt(this.x * this.x + this.y * this.y)
        }
    }

    enum Color { Red, Green, Blue }

    local name = switch (c) {
        Color.Red   -> "red"
        Color.Green -> "green"
        else        -> "?"
    }

    local JSON = import "stdlib/json.art"
    local data = JSON.parse(File.read("config.json"))

## Language

- Variables: `local x = v` for the current scope, `x = v` for globals, `const` for immutable.
- Numbers: `int` and `float`. `int / int` is floor division. Mixed arithmetic promotes.
- Strings: UTF-16, codepoint-indexed. Interpolation `"x is ${x}"`. Multi-line `"""..."""`.
- Tables: `[...]` — array part (1-indexed) and string-keyed hash part, one type.
- Functions: first-class, closures, variadics `...args`, default arguments.
- Classes: inheritance, overloaded methods and operators, getters/setters, static methods. `local` members are private.
- Interfaces: `implements` declares a contract. Verification happens at class definition.
- Enums: named singletons with optional values.
- Switch: expression or statement. No fall-through. `else` is the default.
- Errors: `attempt(fn, ...)` returns `[ok, value_or_message]`. Uncaught errors print `file:line:col: message` with a stack trace.
- Imports: `import "path"` — cached by absolute path, relative to the importing file.
- Operators: `and`/`or` return operands. `a op= b` desugars. User classes can overload `+ - * / % ^ < > <= >= == !=` and unary `-`.
- Types: `x is Int`, `x is SomeClass`, `x is SomeInterface`.
- Membership: `k in table`, `substring in string`, `member in enum`.

## Builtins

    print(v, ...)       tostring(v)         error(msg)         attempt(fn, ...)
    import(path)

    Math.*              abs, floor, ceil, round, trunc, sqrt, pow, exp, log,
                        log2, log10, sin, cos, tan, asin, acos, atan, atan2,
                        min, max, clamp, sign, deg, rad, random, seed
                        pi, tau, e, inf, nan

    String methods      length, charAt, substring, upper, lower, trim, isEmpty,
                        startsWith, endsWith, contains, indexOf, replace,
                        split, reverse, toInt, toFloat,
                        find, match, gmatch, gsub

    Table methods       push, pop, length, isEmpty, contains, indexOf, clear,
                        reverse, clone, keys, values, insert, remove, slice,
                        join, map, filter, reduce, forEach, any, all, find,
                        count, sort

    File                File.read, write, append, delete, open
                        f.read, readLine, readLines, write, append, close

    Time                now, clock, sleep

## Stdlib

Standard library modules written in ART, in `stdlib/`.

    vector2.art
    vector3.art
    json.art

## Layout

    art/                interpreter source (see art/README.md)
    stdlib/             libraries written in ART
    examples/           example scripts
    tests/              test binaries

## Language reference

`CHANGELOG.md` documents every language feature with examples.
It's the authoritative answer to "does ART have X". If you're
looking for syntax, start there.

## Building

    make            # build the interpreter and all tests
    make test       # run the test suite
    make clean      # remove build artifacts

For Clang, CMake, MSVC, or IDE workflows, see `BUILDING.md`.

## License

ART is licensed under the **GNU General Public License, version 3
or later**, with the **ART Runtime Library Exception**.

In short:

- Forks of the interpreter must stay open and GPL-licensed.
- Programs *written in* ART are yours, under any license.
- Applications that *embed* ART are yours, under any license,
  provided the interpreter itself is not modified.

Full text in `LICENSE.md` (GPL v3) and `LICENSE-EXCEPTION.md`
(the exception). `LICENSE-OVERVIEW.md` has the plain-language
summary.

## Contributing

By submitting a pull request you agree to license your contribution
under the same terms as the project: GPL v3 or later, with the
ART Runtime Library Exception.
