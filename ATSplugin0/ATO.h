#ifndef ATO_H
#define ATO_H
// --- ATOのパネルインデックス ---
constexpr int PANEL_ATO_STATE = 137; // ATO状態 (0:非表示, 1:発進待ち, 2:ATO運転中)
constexpr int PANEL_ATO_POWER = 138; // ATO力行指令ノッチ (DigitalNumber)
constexpr int PANEL_ATO_BRAKE = 139; // ATOブレーキ指令ノッチ (DigitalNumber)
constexpr int PANEL_ATO_FAILURE = 140; // ATO故障灯 (PilotLamp)
constexpr int PANEL_ATO_AUTOSTART = 141; // 自動発進待機灯 (PilotLamp)
// --- グローバル変数の外部参照宣言 ---
// 仮設置
extern int g_TASCATOSet;         // 0:手動, 1:TASC, 2:ATO
extern float g_currentLocation;  // 現在位置 (m)
extern int g_maxBrakeNotch;      // 車両の最大常用ブレーキ段数
extern bool g_isAtcPowerOn;      // ATC電源状態
extern bool s_isTascActive;      // TASCモードの有効状態
extern bool g_isAtcFailed;      // ATC故障状態 (1:故障, 0:正常)
extern int g_ATOPower; //ATO力行指令　0で運転士のノッチと同じ　INIファイル対応　ATC電源がオフの時は非表示
extern int g_ATOStart; //ATO発進するかどうか。ATOがオフの時は-1。0で手動、1で自動　ATC電源がオフの時は非表示
extern bool g_ATO_Failure; //ATO故障フラグ　trueで故障
extern bool g_ATO_FailureActive; //ATO故障中かどうか INIファイル対応　trueで故障中
extern float g_ATO_FailureRate; //ATO故障率　0.0～1.0の範囲で設定可能
extern bool g_ATOActive; //ATOが有効かどうか INIファイル対応　trueで有効
extern int g_driverPower; //運転士の力行ノッチ位置
extern int g_driverBrake; //運転士の制動ノッチ位置
extern int g_driverReverser; //運転士のマスコン逆転器位置
extern int g_maxPowerNotch; //車両の最大力行段数
void InitATO();                  // ATO初期化(Loadから呼ぶ)
void ATOOnInitialize();          // 駅ジャンプ・シナリオ開始のとき(Initializeから呼ぶ)
void DisableATO();               // ATOを完全に無効化(g_ATOStart = -1)
void ATOUpdate(bool atcFree, float speedKmh, int atcLimitKmh, float location, int TimeMs);                // ATO更新(毎フレーム呼ぶ)
void ATOOnDoorOpen();           // ドア開時のATO処理
void ATOOnDoorClose();          // ドア閉時のATO処理
void ATOOnStartKey();           // 発進キー押下時のATO処理
void ATOOnAutoStartBeacon();    // 自動発進地上子通過時のATO処理

int ATOGetBrake();
int ATOGetPower();               // ハンドル出力用の力行ノッチ(拡張ノッチのときは P1〜P(ExtendedPowerNotches))
int ATOConvertDriverPower(int driverPower); // 運転士の力行ノッチ → ATOのノッチ(拡張ノッチを使わないときはそのまま)
int ATOGetExtendedPowerNotches(); // INIの [ATO] ExtendedPowerNotches
int ATOGetBrakeStep();           // パネル表示用のブレーキ段数(拡張ノッチのときも 1〜ExtendedNotches)
bool ATOIsAutoStartReady();      // ★追加: 自動発進の待機中か(パネル表示用)
void ATOOnAdvanceNotice(int nextLimitKmh, float distanceM, float location);
void ATOOnSpeedLimitBeacon(int Optional, float location);
void TriggerAtoFailure(); // ★追加: ATO故障を発生させる内部関数
bool ATOCheckRandomFailure(); // ★追加: ATO故障判定関数
void ATOLoadSettings(const char* iniPath);   // ATS.ini の [ATO] から調整値を読み込む
#endif // ATO_H