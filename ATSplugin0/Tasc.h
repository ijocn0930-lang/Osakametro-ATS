#ifndef TASC_H
#define TASC_H

#include <windows.h>

constexpr int PANEL_TASC_NOTCH = 135; // TASCブレーキ指令段数 (DigitalNumber)
constexpr int PANEL_POSITION_ENABLE = 136; // 定点停止灯 (PilotLamp)
// --- グローバル変数の外部参照宣言 ---
extern int g_TASCATOSet;         // 0:手動, 1:TASC, 2:ATO
extern bool g_PositionEnable;    // 停車位置許容範囲フラグ
extern int g_TASCBrakeNotch;     // TASC/ATO出力ノッチ
extern int g_maxBrakeNotch;      // 車両の最大常用ブレーキ段数
extern bool g_isAtcPowerOn;      // ATC電源状態
extern bool s_isTascActive;      // TASCモードの有効状態
extern float g_currentLocation;  // 現在位置 (m)
extern int g_extendedNotches;        // 拡張ノッチ数 (iniから読み込み)
// --- TASC専用関数の宣言 ---
int GetCurrentTascBrakeNotch(); // TASC出力中のノッチ数(0〜14)を取得
void InitTasc(); // TASC内部状態の初期化
int CalculateTascBrake(float currentLocation, float currentSpeed, int driverBrake, int currentTimeMs); // TASCブレーキ演算処理
void OnDoorOpen(); // ドアが開いたらTASCをリセットする
bool IsTascActive(); // ★追加: TASC作動中フラグの取得関数
void ProcessTascBeacon(int type, int optional); // ★追加: 地上子データの処理
#endif // TASC_H