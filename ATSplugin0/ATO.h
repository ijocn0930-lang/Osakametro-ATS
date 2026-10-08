#ifndef ATO_H
#define ATO_H
#include <windows.h>

// --- グローバル変数の外部参照宣言 ---
extern int g_TASCATOSet;         // 0:手動, 1:TASC, 2:ATO
extern float g_currentLocation;  // 現在位置 (m)
extern int g_maxBrakeNotch;      // 車両の最大常用ブレーキ段数
extern bool g_isAtcPowerOn;      // ATC電源状態
extern bool s_isTascActive;      // TASCモードの有効状態
extern int g_isFailed;           // ATC故障状態 (1:故障, 0:正常)
extern bool g_isATCFailed;      // ATC故障状態 (1:故障, 0:正常)
extern int g_ATOPower; //ATO力行指令　0で運転士のノッチと同じ　INIファイル対応　ATC電源がオフの時は非表示
extern int g_ATOStart; //ATO発進するかどうか。ATOがオフの時は-1。0で手動、1で自動　ATC電源がオフの時は非表示
extern bool g_ATO_Failure; //ATO故障フラグ　trueで故障
extern bool g_ATO_FailureActive; //ATO故障中かどうか INIファイル対応　trueで故障中
extern float g_ATO_FailureRate; //ATO故障率　0.0～1.0の範囲で設定可能
extern bool g_ATOActive;
void DisableATO(); //TASCが有効でないときは、ATOも無効化する
void ATOPower(); //ATO力行指令構成
void ATOBrake(); //ATO制動指令構成
void InitATO(); //ATO初期化
#endif // ATO_H