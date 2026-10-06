#include "ATO.h"
#include "Tasc.h"
#include <math.h>
#include <vector>
#include <stdlib.h>
#include <time.h>

//TASCが有効でないときは、ATOも無効化する
void DisableATO() {
    g_ATOPower = 0;
    g_ATOStart = -1;
}