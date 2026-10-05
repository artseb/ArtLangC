#ifndef ART_REGISTRY_H
#define ART_REGISTRY_H

#include "feature.h"

// Feature headers. Each provides the single include for its
// feature plus the extern declaration of its Feature struct.
#include "misc/misc.h"
#include "math/math.h"
#include "number/number.h"
#include "string/string.h"
#include "table/table.h"
#include "function/function.h"
#include "class/class.h"
#include "enum/enum.h"
#include "const/const.h"
#include "switch/switch.h"
#include "interface/interface.h"
#include "file/file.h"
#include "time/time.h"
#include "import/import.h"

// ============================================================
// FEATURES — the feature registry list
//
// Single source of truth for the registry. Adding a feature
// means:
//   1. One include line above.
//   2. One X() entry here.
//
// registry.c expands this to produce the extern declarations and
// the g_features[] array. Order matters: it is the hook priority
// (first feature to claim an event wins) AND the order in which
// register_builtins runs.
// ============================================================

#define FEATURES(X) \
    X(class)        \
    X(enum)         \
    X(const)        \
    X(switch)       \
    X(interface)    \
    X(string)       \
    X(table)        \
    X(function)     \
    X(number)       \
    X(misc)         \
    X(math)         \
    X(file)         \
    X(time)         \
    X(import)

// Register every feature's builtins into S. Called by art_open
// and by test setup. Safe to call more than once — rebinding a
// name overwrites.
void art_register_builtins(ArtState *S);

#endif // ART_REGISTRY_H
