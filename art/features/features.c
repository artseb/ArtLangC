// ============================================================
// features.c — the feature registry
//
// Expanded from FEATURES(X) in features.h. Never edit this file
// to add a feature — edit the list in features.h instead.
// ============================================================

#include "features.h"

#define FEATURE_EXTERN(name) extern Feature name##_feature;
FEATURES(FEATURE_EXTERN)
#undef FEATURE_EXTERN

static Feature *all_features[] = {
#define FEATURE_PTR(name) &name##_feature,
    FEATURES(FEATURE_PTR)
#undef FEATURE_PTR
};

Feature **g_features = all_features;
int g_feature_count = sizeof(all_features) / sizeof(all_features[0]);

void art_register_builtins(ArtState *S)
{
    FEATURES_FOR_EACH(f)
    {
        if (f->register_builtins)
            f->register_builtins(S);
    }
}
