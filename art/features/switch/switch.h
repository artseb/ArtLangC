#ifndef ART_SWITCH_H
#define ART_SWITCH_H

#include "ast.h"
#include "value.h"
#include "state.h"

typedef struct Parser Parser;

typedef struct SwitchArm
{
    Node **cases;
    int case_count;
    Node *body;
} SwitchArm;

typedef struct SwitchNode
{
    Node base;
    Node *subject;
    SwitchArm *arms;
    int arm_count;
    Node *else_body;
} SwitchNode;

Node *node_switch(int line, int col, Node *subject,
                  SwitchArm *arms, int arm_count, Node *else_body);

Node *switch_parse(Parser *P);
bool switch_free_children(Node *n);
const char *switch_node_name(NodeType t);
Value eval_switch(ArtState *S, Node *n);

typedef struct Feature Feature;
extern Feature switch_feature;

#endif
