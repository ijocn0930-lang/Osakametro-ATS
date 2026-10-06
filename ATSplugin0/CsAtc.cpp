#include <windows.h>
#include <stdlib.h>
#include "atsplugin.h"

// Ats.cpp側にある共通のグローバル変数を参照します いずれも未使用
extern int g_maxBrakeNotch; // ブレーキ最大段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
extern bool g_isBuzzerPlaying; // ブザー鳴動中かどうかを記憶するフラグ

// CS-ATCモード（千日前線・長堀鶴見緑地線・今里筋線など・メトロ総合プラグイン準拠）の信号段（インデックス）から速度を割り出す関数
int GetCsAtcSpeed(int signal)
{
    switch(signal) {
        case 10: return 0;   // 停止扱い（0km/h）
case 11: return 0;
case 12: return 0;
case 13: return 15;  // 15km/h
case 14: return 20;  // 20km/h
case 15: return 25;  // 25km/h
case 16: return 30;  // 30km/h
case 17: return 35;  // 35km/h
case 18: return 40;  // 40km/h
case 19: return 45;  // 45km/h
case 20: return 50;  // 50km/h
case 21: return 55;  // 55km/h
case 22: return 60;  // 60km/h
case 23: return 65;  // 65km/h
case 24: return 70;  // 70km/h
case 25: return 75;  // 75km/h
case 26: return 80;  // 80km/h
case 27: return 85;  // 85km/h
case 28: return 90;  // 90km/h
case 29: return 95;  // 95km/h
case 30: return 0;
case 31: return 0;
case 32: return 0;
case 33: return 0;
case 34: return 0;
case 35: return 0;
case 36: return 0;
case 37: return 0;
case 38: return 0;
case 39: return 0;
case 40: return 0;
case 41: return 0;
case 42: return 0;
case 43: return 25; // 構内信号（25km/h）
    }
    return 0;
}
