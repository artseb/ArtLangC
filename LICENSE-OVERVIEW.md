# Licensing

ART is licensed under the **GNU General Public License, version 3
or later**, with the **ART Runtime Library Exception**.

Three files together form the license:

- **`LICENSE.md`** — the GNU General Public License, version 3.
  Copied verbatim from the FSF. This is the base license and
  governs everything the exception does not explicitly relax.
- **`LICENSE-EXCEPTION.md`** — the ART Runtime Library Exception.
  An additional permission under section 7 of the GPL v3. It
  grants the right to link ART into an application without
  requiring that application to be GPL-licensed.
- **`LICENSE-OVERVIEW.md`** — this file. A plain-language summary.
  Informational only; it does not modify either of the above.

The exception does not modify the GPL v3. It sits on top of it,
granted by the copyright holder, and only relaxes specific terms.
Where the two ever disagree, the GPL v3 governs.

## What this means in practice

**Forks of the interpreter.** Any modification or redistribution
of ART — the C source under `art/`, the static library, the `art`
binary — must remain under the same terms, source available. A
closed-source fork is not permitted. The exception does not
relax this.

**Programs written in ART.** Scripts, modules, and applications
written in the ART language are not covered by the license of the
interpreter that runs them. This mirrors the rule that allows
proprietary shell scripts under GPL bash. Your `.art` files are
yours, under any license you choose.

**Applications that embed ART.** An application that links against
ART — a game engine calling the C API in `art.h`, a tool embedding
the interpreter, a plugin loading the static library — may be
distributed under terms of its own choosing, including proprietary
terms, provided that the interpreter itself is not modified and
the application qualifies as an Independent Module under the
exception.

**Modifications to the interpreter used inside a proprietary
application.** If you patch `eval_call.c`, extend `gc.c`, or
otherwise modify ART, and then ship the result inside your engine,
the modification is a derivative of ART and must be released under
GPL v3. The rest of your engine remains proprietary. Only the
ART-modifying part must be free.

## SPDX identifier

Source files in `art/` carry this header, which is the
machine-readable statement that a given file is covered by both
the GPL v3 and the exception:

```c
// SPDX-License-Identifier: GPL-3.0-or-later WITH ART-Runtime-Exception
//
// Copyright (C) 2026 <copyright holder>
//
// This file is part of ART.
//
// ART is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version, together with the ART Runtime
// Library Exception.
//
// ART is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with ART. If not, see <https://www.gnu.org/licenses/>.
```

`ART-Runtime-Exception` is a custom SPDX identifier. If SPDX
registers a formal one for this pattern in the future, the line
would be updated to reference it. Until then, the identifier is
informational — the authoritative statements are the text of the
GPL v3 and `LICENSE-EXCEPTION.md`.

The header is not required in `.art` scripts, in `tests/`, or in
documentation. It belongs on the licensed source of the
interpreter.

## Dual licensing

Nothing in the GPL v3 prevents the copyright holder from also
offering ART under a separate commercial license to parties who
prefer not to accept the GPL terms. That is a separate agreement
and does not affect the public license described here.

## Attribution

The Runtime Library Exception is modeled on the **GCC Runtime
Library Exception, version 3.1** and the **Java Classpath
Exception**, both of which use the same section 7 mechanism. The
wording here is adapted to name ART in place of those projects.

## Not legal advice

This document is a plain-language summary of the license and does
not modify the GPL v3 or the Runtime Library Exception. In case of
conflict, the GPL v3 text governs. If the licensing matters to
your organization, have counsel review it.

</BDS:create_file>

<BDS:create_file fileName="LICENSE-EXCEPTION.md">

```markdown
# ART Runtime Library Exception

Version 1.0

The GNU General Public License, version 3 (the "GPL v3") is in
`LICENSE.md`. This file is an additional permission granted under
section 7 of the GPL v3 by the copyright holder of ART. It applies
to every file in the ART source tree that bears the SPDX identifier
`GPL-3.0-or-later WITH ART-Runtime-Exception`.

When you link ART with a program, ART may be combined with that
program to produce an executable. The purpose of this Exception
is to allow linking of non-GPL (including proprietary) programs
to use ART in this way.

## 0. Definitions.

A file is an **Independent Module** if it either requires ART for
execution after linking, or makes use of an interface provided by
ART, but is not otherwise based on ART.

**"ART"** means a version of the ART interpreter, with or without
modifications, governed by version 3 (or a specified later version)
of the GNU General Public License with the option of using any
subsequent versions published by the FSF.

**"GPL-compatible Software"** is software whose conditions of
propagation, modification and use would permit combination with
ART in accord with the license of ART.

## 1. Grant of Additional Permission.

You have permission to link ART with Independent Modules to
produce an executable, regardless of the license terms of these
Independent Modules, and to copy and distribute the resulting
executable under terms of your choice, provided that you also
meet, for each linked Independent Module, the terms of the license
of that module. An Independent Module is a module which is not
derived from or based on ART.

## 2. No Weakening of ART Copyleft.

The availability of this Exception does not imply any presumption
that third-party software is generally permitted to link with ART
or with any other program to which this Exception applies. In
particular, this Exception does not permit you to combine ART
with any program that is not an Independent Module, except as
permitted by GPL v3.

