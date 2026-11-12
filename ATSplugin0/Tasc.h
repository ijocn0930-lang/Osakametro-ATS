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
extern bool g_TASC_Failure;      // TASC故障状態 (1:故障, 0:正常)
extern float g_TASC_FailureRate; // TASC故障確率 (0.0〜1.0)
extern bool g_TASC_FailureActive; // TASC故障判定有効フラグ (trueで確率判定を行う)
extern float TASC_DECEL_MS2; // TASC目標減速度 (m/s^2)
extern float g_defaultClearance; // デフォルトの許容範囲 (m)
extern int g_maxPowerNotch; //車両の最大力行段数
constexpr float GRADE_ACCEL_MS2_PER_PERMILLE = 0.0088f; // 1‰の勾配での加速度 (m/s^2)
constexpr int PANEL_INCHING = 142; // インチング中表示灯 (PilotLamp)
// --- TASC専用関数の宣言 ---
int GetCurrentTascBrakeNotch(); // TASC出力中のノッチ数(0〜14)を取得
void InitTasc(); // TASC内部状態の初期化
void TascOnDoorClose(); // ドアが閉まったとき(INIの設定で転動防止ブレーキを緩める)
void TascArmHold(); // シナリオ開始・駅ジャンプ・モード切替のあと、止まっていれば転動防止ブレーキを掛ける
int CalculateTascBrake(float currentLocation, float currentSpeed, int driverBrake, int currentTimeMs); // TASCブレーキ演算処理
void OnDoorOpen(); // ドアが開いたらTASCをリセットする
void TascOnStationLampBeacon(int optional); // 地上子1041(始発駅・駅ジャンプのあとの定点停止灯)
void TascTrackStartLamp(float location, float speed); // 毎フレーム: 1041の位置から動いたら取り消す
bool IsTascActive(); // ★追加: TASC作動中フラグの取得関数
void ProcessTascBeacon(int type, int optional); // ★追加: 地上子データの処理
bool CheckRandomFailure(); // ★追加: TASC故障確率判定関数
void TriggerTascFailure(); // ★追加: TASC故障を発生させる内部関数
void OnTascSwitchToggled(int newState); // ★追加: TASCスイッチの切り替え処理
void ProcessGradeBeacon(int optional, float location); // ★追加: こう配地上子(1008)の処理
float GetAverageGrade(float from, float to);           // ★追加: 区間の平均こう配(‰)。のぼりがプラス
void ResetGrade();                                     // ★追加: こう配情報の消去
void TascLoadSettings(const char* iniPath);            // ★追加: ATS.ini の [TASC] から調整値を読み込む
// --- インチング ---
bool TascInchingUpdate(bool keyHeld, bool atcFree, float location, float speedKmh,
    int driverBrake, int driverPower, int driverReverser); // 毎フレーム呼ぶ。trueのとき下の値を出力に使う
int  TascInchingPower();     // インチングの力行ノッチ
int  TascInchingBrake();     // インチングのブレーキノッチ(動かしている間は0)
int  TascInchingReverser();  // インチングのリバーサー(+1前進、-1後進)
bool TascIsInching();        // インチング中か(パネル表示用)


#endif // TASC_H