/*
 * Shared by the screens around the fight (VS, CONTINUE, CREDITS, ...): the 3D "stage" (an
 * arena drawn outside a fight, code at 0x8002EA90) and the globals they all use.
 */
#ifndef SCREEN_H
#define SCREEN_H

#include "combat.h"

void func_8002EA90(void *stage);           /* the 3D stage: set up, */
void func_8002ECBC(void *stage);           /* ... free, */
void func_8002ED34(void *stage);           /* ... update, */
void func_8002FD50(void *stage, void *cam);/* ... draw (two passes) */
void func_8002FE34(void *stage, void *cam);

#endif
