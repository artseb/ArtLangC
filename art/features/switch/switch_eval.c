#include "switch.h"
#include "interp.h"
#include <stdlib.h>

static bool same_value_type(Value a, Value b)
{
    if (a.type != b.type) return false;
    if (a.type != VAL_OBJ) return true;
    return AS_OBJ(a)->type == AS_OBJ(b)->type;
}

Value eval_switch(ArtState *S, Node *n)
{
    SwitchNode *sw = (SwitchNode *)n;

    Value subject = art_eval(S, sw->subject);
    if (S->control != CONTROL_NONE)
        return NIL_VAL;

    int total = 0;
    for (int i = 0; i < sw->arm_count; i++)
        total += sw->arms[i].case_count;

    Value *vals = NULL;
    if (total > 0)
        vals = malloc(sizeof(Value) * total);

    int k = 0;
    for (int i = 0; i < sw->arm_count; i++)
    {
        SwitchArm *arm = &sw->arms[i];
        for (int j = 0; j < arm->case_count; j++)
        {
            Value c = art_eval(S, arm->cases[j]);
            if (S->control != CONTROL_NONE) { free(vals); return NIL_VAL; }

            if (!same_value_type(c, subject))
            {
                free(vals);
                art_runtime_error(S, arm->cases[j],
                                  "case type %s doesn't match switch type %s",
                                  value_type_name(c), value_type_name(subject));
            }

            vals[k++] = c;
        }
    }

    k = 0;
    for (int i = 0; i < sw->arm_count; i++)
    {
        SwitchArm *arm = &sw->arms[i];
        for (int j = 0; j < arm->case_count; j++)
        {
            if (value_equal(vals[k++], subject))
            {
                free(vals);
                return art_eval(S, arm->body);
            }
        }
    }

    free(vals);

    if (sw->else_body)
        return art_eval(S, sw->else_body);
    return NIL_VAL;
}
