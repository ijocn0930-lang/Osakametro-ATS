#include "ATO.h"
#include "Tasc.h"
#include "atsplugin.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <vector>
#include <windows.h>

// --- 外部ファイル（CsAtc.cpp）にある関数を宣言 ---
extern int GetCsAtcSpeed(int signal);

// --- 地上子タイプ定義 ---
const int BEACON_TYPE_LINE_CHECK = 20; // 号線照合用地上子のType番号
const int BEACON_TYPE_ADVANCE_NOTICE = 31;  // 前方予告用地上子 (21 -> 31へ修正)
float TASC_DECEL_MS2 = 0.6944f;        // 目標減速度 (既定2.5 km/h/s)

// --- グローバル変数 ---
 // 現在の閉塞から取得したATC制限速度 (km/h)
int g_currentAtcSpeed = 0;   // 現在の閉塞から取得したATC制限速度 (km/h)

int g_lastRawSignal = 0;     // BVEから最後に送られてきた生の信号インデックスを記憶する
int g_receivedSignal = 0;        // BVEから最後に送られてきた信号インデックスを記憶する

int g_lastCsLimitSpeed = -1;  // 前回のCS-ATC制限速度を記憶する変数（初期値は-1）

float g_atcLimitSpeed = 0.0f;  // 現在の信号ケースに対応する指示速度 (km/h)

int g_currentSignalCase = 0;   // 現在の信号ケース (0, 10, 11, 12, 13...)
float g_currentLocation = 0.0f; // 現在位置 (m)
float g_location = 0.0f; // 現在位置 (m) --- IGNORE ---
int g_BrakeNotch = 0; // 運転士が現在セットしているブレーキ段数

// 【新規】WS用とCS用の制限速度を別々に独立して保持する変数
int g_wsLimitSpeed = 0;
int g_csLimitSpeed = 0;


// 運転士の現在のハンドル位置を保持する変数
int g_driverPower = 0; // 運転士が現在セットしている力行段数
int g_driverBrake = 0; // 運転士が現在セットしているブレーキ段数
int g_driverReverser = 0; // 運転士が現在セットしているレバーの前後進位置（-1=後進, 0=ニュートラル, 1=前進）
int g_driverConstantSpeed = 0; // 運転士が現在セットしている定速段数
int g_MaxBrakeCNotch = 0; // 常用最大ブレーキ段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
int g_maxPowerNotch = 0; // 車両の最大力行段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
int g_atsReverserNotch = 0; // ATSが現在セットしているレバーの前後進位置（-1=後進, 0=ニュートラル, 1=前進）
int g_atsBrakeNotch = 0;
int g_brake67Notch = 0;
int g_cars = 0;
float g_speed = 0.0f; // 現在の速度 (km/h)
// ATCの起動状態（初期状態は false）
// 保安装置の状態

bool g_isOverrideMode = false;  //非設
bool g_isEmergencyOperationMode = false;  //非常運転モード
bool g_isYardMode = false;       // 構内モード
int g_yardLimitSpeed = 25;       // 構内モード制限速度 (km/h)
bool g_isAtcPowerOn = false;    //ATC電源
bool g_isBuzzerPlaying = false; // ブザー鳴動中かどうかを記憶するフラグ
bool g_wasPowerOffLastFrame = true; // 前フレームの電源オフ状態を記憶するフラグ
bool g_shouldPlayBell = false; // ベルを鳴らす指示を伝えるフラグ
bool g_isOverrideWarningPlaying = false; // 非設モード警報音が鳴動中かどうかを記憶するフラグ

// ATC故障関連パラメータ・変数
bool g_atcFailSetting = false;  // INIから読み込む故障設定フラグ
float g_failParameter = 0.0f;    // INIから読み込む故障確率 (0.0～1.0)
bool g_isAtcFailed = false;      // 現在ATCが故障中かどうか

// 外部INIファイルから読み込む車両の仕様設定
int g_maxBrakeNotch = 6; // ブレーキ最大段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
int g_atcTypeSetting = 2; // 0=WS固定, 1=CS固定, 2=両方
bool g_tascEnabled = true; // この車両でTASCモードを使えるか INIファイル対応 ([VehicleSpec] TascEnable)
bool g_atoEnabled = true;  // この車両でATOモードを使えるか INIファイル対応 ([VehicleSpec] AtoEnable)


// 現在選択されているアクティブなATCモード (false = WS-ATC, true = CS-ATC)
bool g_isCurrentModeCS = false;

// 【新規】号線切替用変数
int g_defaultLine = 1;        // デフォルト号線
int g_minLine = 1;            // 最小号線
int g_maxLine = 8;            // 最大号線
int g_currentLine = 1;        // 現在選択中の号線 (1〜10)
bool g_isLineMismatch = false;// 【新規】号線不一致/不正検出フラグ (trueで非常ブレーキ固縛)

// 【新規】他社線・直通モード識別フラグ
bool g_isKitakyu = false;   // 北大阪急行モード
bool g_isMidosuji = false;  // 御堂筋線モード
bool g_isKeihan = false;    // 近鉄けいはんな線モード
bool g_isChuo = false;      // 中央線モード
int g_currentRouteCode = 0; // 受信した直通モードコードの記憶用
// 直前の位置（駅ジャンプ検知用）
float g_lastLocation = -1.0f;
// 長田駅（境界駅）判定用フラグ
bool g_isInNagataStation = false;
// ATCブレーキ介入状態表示フラグ
bool g_isAtcServiceBrake = false;   // ATC常用パターン接近・介入
bool g_isAtcEmergencyBrake = false; // ATC非常ブレーキ作動（02信号）
bool g_isAdvanceNoticeActive = false; // 前方予告作動中フラグ

//TASC ATO関連変数
int g_TASCATOSet = 0; //TASC/ATOの有効化 0で手動 INIファイル対応　ATC電源がオフの時は非表示。不具合回避のため、オンにするときは手動をセットする。
int g_Position = 0; //debug--停車位置までの距離 cm単位で1桁ずつ分ける 999.99mまで。これ以上は不要　ATC電源がオフの時は非表示
bool g_PositionEnable = false; //停車位置の認識 手動でもtrueにする。ATC電源がオフの時は非表示
int g_TASCBrakeNotch = 0; //TASC制御・ATO制動指令で使用するブレーキノッチ　0で運転士のノッチと同じ INIファイル対応　ATC電源がオフの時は非表示
bool g_TASCControl = false; //TASC制御中かどうか　電源オフの時は非表示
bool g_ATOActive = false; //ATOがアクティブかどうか。
int g_ATOPower = 0; //ATO力行指令　0で運転士のノッチと同じ　INIファイル対応　ATC電源がオフの時は非表示
int g_ATOStart = 0; //ATO発進するかどうか。ATOがオフの時は-1。0で手動、1で自動　ATC電源がオフの時は非表示
bool g_inching = false; //オーバーランしたときにインチングします。（インチングはキー押下のみ手動）
const int BEACON_TASC_POSITION = 1030; //停車位置の設定　TASC制御地上子含む　センチメートル単位
const int BEACON_TASC_POSITIONRANGE = 1031; //許容範囲 センチメートル単位
const int BEACON_LEVEL = 1008; //こう配補正
const int BEACON_ATOAUTOSTART = 1003; //自動発進
const int BEACON_SPEEDLIMIT = 1007; //速度制限
const int BEACON_PLATFORMDOOR = 1040; //ホームドア(可動柵)の有無 Optional 1:あり 0:なし
const int BEACON_STATIONLAMP = 1041; //始発駅・駅ジャンプのあとの定点停止灯 停車位置に置く Optional 許容範囲cm(0は既定値)

// --- ホームドア(可動柵)連動パネル ---
bool g_psdEnable = true;          // 車両がホームドア連動に対応しているか INIファイル対応 ([PlatformDoor] Enable)
bool g_psdDefault = false;        // シナリオ開始・駅ジャンプのあと、1040を通るまでのホームドアの有無 ([PlatformDoor] DefaultStation)
int  g_psdPanelOpen = 40;         // 可動柵開のパネル番号 ([PlatformDoor] PanelOpen)
bool g_psdStationHas = false;     // 次に止まる駅にホームドアがあるか(地上子1040で設定)
bool g_isDoorOpen = true;         // 車両のドアが開いているか(BVE開始時は開)
// ホームドアの開閉の動き(車両のドアが開いたら開き、車両のドアが閉まったあとに閉まる)
enum PsdState { PSD_CLOSED = 0, PSD_OPENING = 1, PSD_OPEN = 2, PSD_CLOSING = 3 };
int   g_psdState = PSD_CLOSED;     // ホームドアの状態
int   g_psdStateUntilMs = 0;       // 今の動き(待ち・開閉中)が終わる時刻
bool  g_psdWaitOpen = false;       // 車両のドアが開いた: OpenDelay のあとに開き始める
bool  g_psdWaitClose = false;      // 車両のドアが閉まった: CloseDelay のあとに閉まり始める
bool  g_psdInstant = true;         // シナリオ開始・駅ジャンプの直後(このときは動きなしで開いた状態にする)
bool  g_psdDoorOpenEvent = false;  // DoorOpen を受けた(次の Elapse で処理する)
bool  g_psdDoorCloseEvent = false; // DoorClose を受けた(次の Elapse で処理する)
float g_psdOpenDelay = 0.0f;       // 車両のドアが開いてから、ホームドアが開き始めるまで(秒) ([PlatformDoor] OpenDelay)
float g_psdOpenTime = 3.0f;        // ホームドアが開き終わるまで(秒) ([PlatformDoor] OpenTime)
float g_psdCloseDelay = 1.0f;      // 車両のドアが閉まってから、ホームドアが閉まり始めるまで(秒) ([PlatformDoor] CloseDelay)
float g_psdCloseTime = 3.0f;       // ホームドアが閉まり終わるまで(秒) ([PlatformDoor] CloseTime)
int   g_psdSoundOpen = -1;         // 開き始めるときの音の番号(-1で鳴らさない) ([PlatformDoor] SoundOpen)
int   g_psdSoundClose = -1;        // 閉まり始めるときの音の番号 ([PlatformDoor] SoundClose)
int   g_psdPanelState = -1;        // 状態のパネル番号(-1で使わない) 0:閉 1:開いている途中 2:開 3:閉まっている途中 ([PlatformDoor] PanelState)

// --- 母線引き通し表示灯 ---
//   サードレールのない区間(絶縁・分岐器)を地上子1044で知らせる。M車の集電靴が1つでも区間に入ると消灯する
const int BEACON_NORAIL = 1044;   // サードレールのない区間 Optional: この地上子からの区間の長さ(cm)
const int BUS_SHOE_MAX = 32;
const int BUS_GAP_MAX = 32;
bool  g_busEnable = true;         // ([BusLine] Enable)
int   g_busPanel = 197;           // 表示灯のパネル番号 ([BusLine] Panel)
float g_busOnSpeed = 5.0f;        // この速度(km/h)以上で点灯(doorLock.dll から引き継ぎ) ([BusLine] OnSpeed)
// --- 停電 ---
//   地上子1046で停電区間を知らせる(Optional 1: ここから停電、0: ここで復旧)
//   停電中: 架線電圧 0V、電流計 0A、母線引き通し表示灯は消灯、力行しない
const int BEACON_OUTAGE = 1046;
bool g_powerOutage = false;
bool g_outageCutRegen = true;     // 停電中はレバーサーを中立にして、電気(回生)ブレーキを止める ([Outage] CutRegen)
// --- 回生失効 ---
//   回生ブレーキ中に架線電圧が上限を超えたら回生失効。ブレーキを緩めるまで電気ブレーキを止める(空気ブレーキだけ)
bool  g_regenFailEnable = true;   // ([RegenFail] Enable)
float g_regenFailLimit = 790.0f;  // この架線電圧(V)を超えたら回生失効 ([RegenFail] Limit)
int   g_regenFailPanel = -1;      // 回生失効の表示灯のパネル番号(-1で使わない) ([RegenFail] Panel)
bool  g_regenFail = false;        // 回生失効中か
float g_busShoes[BUS_SHOE_MAX];   // M車の集電靴の位置(先頭からの距離 m) ([BusLine] ShoePositions)
int   g_busShoeCount = 0;
float g_busGapStart[BUS_GAP_MAX]; // サードレールのない区間(距離程 m)
float g_busGapEnd[BUS_GAP_MAX];
int   g_busGapCount = 0;

// --- 分けた画像のメーター(8192pxを超える DigitalNumber を、何枚かの画像に分けて重ねる) ---
//   GaugeImages\SplitStrip.ps1 で分けた画像用。どの画像も0番目のコマは空、1番目以降がその画像の受け持つ値
int g_splitPanel = -1;            // 最初のパネル番号(-1で使わない) ([SplitMeter] Panel)
int g_splitParts = 3;             // 画像の枚数 ([SplitMeter] Parts)
int g_splitPerPart = 82;          // 1枚が受け持つ値の数 ([SplitMeter] PerPart)

// --- 現在駅・次駅・キロ程の表示 ---
const int BEACON_STATIONINFO = 1042; // 現在駅と次駅 Optional 現在駅×1000＋次駅 (正:下り 負:上り)。停車位置に置く
const int BEACON_KILOPOST = 1043;    // キロ程 Optional 下り: キロ程(m)−距離程(m)、上り: キロ程(m)＋距離程(m)
bool g_stnEnable = true;          // 表示するか ([StationDisplay] Enable)
int  g_stnPanelStart = 200;       // 駅表示の最初のパネル番号 ([StationDisplay] StationPanelStart)。号線ごとに4つ(下り現在・下り次・上り現在・上り次)
int  g_kiloPanelStart = 240;      // キロ程の最初のパネル番号 ([StationDisplay] KiloPanelStart)。10km・1km・100m・10mの4つ
int  g_stnCurrent = 0;            // 現在駅の番号 (0:なし)
int  g_stnNext = 0;               // 次駅の番号 (0:なし)
bool g_stnUp = false;             // 上りか
bool g_kiloValid = false;         // キロ程を受信したか
int  g_kiloOffset = 0;            // キロ程と距離程の差(m)。下り: キロ程 = 差＋距離程、上り: キロ程 = 差−距離程
bool s_isTascActive = false; //TASC制御中かどうか
int g_extendedNotches = 0; //拡張ノッチ段数　INIファイル対応
float g_defaultClearance = 0.5f; //デフォルトの許容範囲　INIファイル対応
int g_pressureRatesCount = 0; //圧力比率テーブルの要素数
bool g_TASC_Failure = false; //TASC故障フラグ　trueで故障
float g_TASC_FailureRate = 0.0f; //TASC故障率　0.0～1.0の範囲で設定可能
bool g_TASC_FailureActive = false; //TASC故障中かどうか INIファイル対応　trueで故障中
bool g_ATO_Failure = false; //ATO故障フラグ　trueで故障
bool g_ATO_FailureActive = false; //ATO故障中かどうか INIファイル対応　trueで故障中
float g_ATO_FailureRate = 0.0f; //ATO故障率　0.0～1.0の範囲で設定可能

// このDLL自体のインスタンスハンドルを保持する変数
HINSTANCE g_hModule = NULL;
// 号線変更に伴う内部フラグの更新
// 号線番号がけいはんな線や北急線独自割り当て番号（例: 近鉄=10, 北急=9 等）の運用に応じた処理
    // ※ 乗り入れ境界での手動切り替え時は、号線番号に応じて路線属性を確定させます
    // 必要に応じて号線番号の割り当て条件を調整可能です

static void UpdateLineFlagsFromCurrentLine() {
    g_isMidosuji = (g_currentLine == 1);
    g_isChuo = (g_currentLine == 4);
    g_isKitakyu = (g_currentLine == 9);
    // 号線を直接切り替えた場合はけいはんな線モードを解除
    g_isKeihan = false;
}
// CS-ATC保持状態のリセット関数
static void ResetCsAtcState() {
    g_csLimitSpeed = 0;
    g_lastCsLimitSpeed = -1;
    g_isAdvanceNoticeActive = false;
}
// カンマ区切りの文字列から要素数をカウントする関数
static int CountCommaSeparatedElements(const char* str) {
    if (str == NULL || str[0] == '\0') {
        return 0;
    }

    int count = 1; // 文字列が存在すれば最低1要素
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ',') {
            count++;
        }
    }
    return count;
}
// DLLのエントリポイント
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
		g_hModule = hinstDLL; // DLLのインスタンスハンドルを保持
		DisableThreadLibraryCalls(hinstDLL); // スレッドの作成/終了通知を無効化
    }
    return TRUE;
}

// INIの値から、行の後ろのコメント(; 以降)と末尾の空白を取り除く(Windowsはコメントを取り除かないため)
static void StripIniComment(char* buf) {
    char* sc = strchr(buf, ';');
    if (sc != NULL) *sc = '\0';
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) buf[--len] = '\0';
}

// INIから true/false を読む(true, 1 で true)。行の後ろのコメント(; 以降)は取り除く
static bool ReadIniBool(const char* section, const char* key, bool def, const char* iniPath) {
    char buf[128] = { 0 };
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath);
    StripIniComment(buf);
    if (buf[0] == '\0') return def;
    return (_stricmp(buf, "true") == 0 || atoi(buf) != 0);
}

// --- パネルの更新間隔・反映遅延 (INI [PanelTiming] パネル番号 = 更新間隔ms, 反映遅延ms) ---
const int PT_MAX = 32;   // 設定できるパネルの数
const int PT_QUEUE = 64; // 反映待ちの値をためておく数
struct PanelTiming {
    int panel;                  // パネル番号
    int intervalMs;             // 更新間隔(ms)。0で毎フレーム
    int delayMs;                // 反映遅延(ms)。0で遅れなし
    int shown;                  // 表示中の値
    int delayed;                // 反映遅延をかけたあとの値
    int lastUpdateMs;           // 最後に更新した時刻
    int qTime[PT_QUEUE];        // 反映待ちの値(変わった時刻と値)
    int qValue[PT_QUEUE];
    int qHead, qCount;
    int lastInput;              // 前のフレームの元の値
    bool started;
};
static PanelTiming s_pt[PT_MAX];
static int s_ptCount = 0;
static int s_ptLastTimeMs = -1;

static void LoadPanelTiming(const char* iniPath) {
    s_ptCount = 0;
    char buf[4096] = { 0 };
    GetPrivateProfileSectionA("PanelTiming", buf, sizeof(buf), iniPath);
    for (char* p = buf; *p != '\0' && s_ptCount < PT_MAX; p += strlen(p) + 1) {
        char line[256];
        strncpy_s(line, sizeof(line), p, _TRUNCATE);
        StripIniComment(line);
        char* eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        int panel = atoi(line);
        if (panel < 0 || panel > 255) continue;
        int interval = atoi(eq + 1);
        char* comma = strchr(eq + 1, ',');
        int delay = (comma != NULL) ? atoi(comma + 1) : 0;
        if (interval < 0) interval = 0;
        if (delay < 0) delay = 0;
        if (interval == 0 && delay == 0) continue;
        PanelTiming& t = s_pt[s_ptCount++];
        t.panel = panel; t.intervalMs = interval; t.delayMs = delay;
        t.started = false;
    }
}
// パネル出力の最後に呼ぶ: 設定したパネルの値を、更新間隔・反映遅延に合わせて書き換える
static void ApplyPanelTiming(int* panel, int nowMs) {
    bool reset = (s_ptLastTimeMs < 0 || nowMs < s_ptLastTimeMs); // シナリオ開始・駅ジャンプで時刻が戻ったとき
    s_ptLastTimeMs = nowMs;
    for (int i = 0; i < s_ptCount; i++) {
        PanelTiming& t = s_pt[i];
        int input = panel[t.panel];
        if (reset || !t.started) {
            t.started = true;
            t.shown = input; t.delayed = input; t.lastInput = input;
            t.lastUpdateMs = nowMs;
            t.qHead = 0; t.qCount = 0;
            continue;
        }
        // 反映遅延: 値が変わった時刻をためておき、遅延時間がたったものから取り出す
        if (t.delayMs > 0) {
            if (input != t.lastInput) {
                if (t.qCount == PT_QUEUE) { t.qHead = (t.qHead + 1) % PT_QUEUE; t.qCount--; } // あふれたら古いものを捨てる
                int idx = (t.qHead + t.qCount) % PT_QUEUE;
                t.qTime[idx] = nowMs; t.qValue[idx] = input; t.qCount++;
            }
            while (t.qCount > 0 && nowMs - t.qTime[t.qHead] >= t.delayMs) {
                t.delayed = t.qValue[t.qHead];
                t.qHead = (t.qHead + 1) % PT_QUEUE; t.qCount--;
            }
        }
        else {
            t.delayed = input;
        }
        t.lastInput = input;
        // 更新間隔: 前に更新してから間隔がたったときだけ表示を変える
        if (t.intervalMs <= 0 || nowMs - t.lastUpdateMs >= t.intervalMs) {
            if (t.intervalMs > 0) t.lastUpdateMs = nowMs;
            t.shown = t.delayed;
        }
        panel[t.panel] = t.shown;
    }
}

// --- 縦バー形の計器 (INI [Gauge1]〜[Gauge8]) ---
//   バー: 目盛りの範囲(Min〜Max)を Steps 段に分け、0〜Steps のコマ番号を出す(DigitalNumberの画像をコマごとに用意する)
//   数字: 1桁ずつ別のパネルに出す。0=非表示 1〜10=数字0〜9。上の桁の0は非表示
const int GAUGE_MAX = 8;
struct GaugeSetting {
    int source;        // 0:なし 1:BC圧 2:MR圧 3:ER圧 4:BP圧 5:SAP圧(kPa) 6:電流(A) 7:架線電圧(V) 8:制御電圧(V)
    float minValue, maxValue;
    int steps;         // バーの段数(1段=1ピクセルにするなら、目盛りの高さのピクセル数)
    int barPanel;      // バーのパネル番号(-1で使わない)。Segments 個使う(下から順に)
    int segments;      // バーを縦に何枚に分けるか(画像の高さの上限8192pxを超えないようにするため)。1で分けない
    int markerHalf;    // マーカーの高さの半分(px)。高さ5pxなら2
    int digitPanel;    // 数字の最上位の桁のパネル番号(-1で使わない)。ここから Digits 個使う
    int digits;        // 数字の桁数(1〜5)
};
static GaugeSetting s_gauges[GAUGE_MAX];
// 架線電圧(ATSプラグインには渡されないので、電流から計算する)
static float s_lvNominal = 750.0f;    // 無負荷のときの電圧(V)
static float s_lvDropPerA = 0.05f;    // 力行電流1Aあたりの電圧降下(V)
static float s_lvRisePerA = 0.03f;    // 回生電流1Aあたりの電圧上昇(V)
static float s_lvMin = 650.0f, s_lvMax = 800.0f;
// ゆらぎ(停車中も少しずつ変わる): ときどき新しい目標値を選び、そこへゆっくり近づける
struct Fluctuation {
    float amp = 0.0f;       // ゆらぎの大きさ(±)。0でゆらがない
    float period = 3.0f;    // 次の目標値を選ぶまでの平均の時間(秒)
    float now = 0.0f;       // 今のゆらぎ
    float target = 0.0f;    // 目標のゆらぎ
    int nextMs = -1;        // 次に目標を選ぶ時刻
    int lastMs = -1;
};
static Fluctuation s_lvFluct;   // 架線電圧のゆらぎ(ほかの列車の加減速など)
// 変電所(地上子1045で位置を知らせる)。変電所に近いほど電圧が高く、離れるほど力行での電圧降下が大きい
const int SUBSTATION_MAX = 64;
static float s_subPos[SUBSTATION_MAX];  // 変電所の位置(距離程 m)
static int   s_subCount = 0;
static float s_subRise = 30.0f;         // 変電所の真横での電圧の上がり(V) ([LineVoltage] SubstationRise)
static float s_subRange = 2000.0f;      // 変電所の効きが届く距離(m) ([LineVoltage] SubstationRange)
void AddSubstation(float pos) {
    for (int i = 0; i < s_subCount; i++) if (fabsf(s_subPos[i] - pos) < 1.0f) return;  // 同じ変電所は1回だけ
    if (s_subCount < SUBSTATION_MAX) s_subPos[s_subCount++] = pos;
}
// 制御電圧(SIV・蓄電池からの直流。ATSプラグインには渡されないので、設定値にゆらぎを足して出す)
static float s_cvNominal = 100.0f;    // ふだんの電圧(V)
static float s_cvMin = 0.0f, s_cvMax = 150.0f;
static Fluctuation s_cvFluct;

static float IniFloat(const char* section, const char* key, float def, const char* iniPath) {
    char buf[64] = { 0 };
    GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), iniPath);
    StripIniComment(buf);
    if (buf[0] == '\0') return def;
    return (float)atof(buf);
}
static void LoadGauges(const char* iniPath) {
    for (int i = 0; i < GAUGE_MAX; i++) {
        char sec[16];
        sprintf_s(sec, sizeof(sec), "Gauge%d", i + 1);
        GaugeSetting& g = s_gauges[i];
        char src[32] = { 0 };
        GetPrivateProfileStringA(sec, "Source", "", src, sizeof(src), iniPath);
        StripIniComment(src);
        g.source = 0;
        if (_stricmp(src, "bc") == 0) g.source = 1;
        else if (_stricmp(src, "mr") == 0) g.source = 2;
        else if (_stricmp(src, "er") == 0) g.source = 3;
        else if (_stricmp(src, "bp") == 0) g.source = 4;
        else if (_stricmp(src, "sap") == 0) g.source = 5;
        else if (_stricmp(src, "current") == 0) g.source = 6;
        else if (_stricmp(src, "voltage") == 0) g.source = 7;
        else if (_stricmp(src, "control") == 0) g.source = 8;
        g.minValue = IniFloat(sec, "Min", 0.0f, iniPath);
        g.maxValue = IniFloat(sec, "Max", 1000.0f, iniPath);
        if (g.maxValue <= g.minValue) g.maxValue = g.minValue + 1.0f;
        g.steps = GetPrivateProfileIntA(sec, "Steps", 50, iniPath);
        if (g.steps < 1) g.steps = 1;
        g.segments = GetPrivateProfileIntA(sec, "Segments", 1, iniPath);
        if (g.segments < 1) g.segments = 1;
        if (g.segments > 8) g.segments = 8;
        g.markerHalf = GetPrivateProfileIntA(sec, "MarkerHalf", 2, iniPath);
        if (g.markerHalf < 0) g.markerHalf = 0;
        g.barPanel = GetPrivateProfileIntA(sec, "BarPanel", -1, iniPath);
        if (g.barPanel + g.segments > 256) g.barPanel = -1;
        g.digits = GetPrivateProfileIntA(sec, "Digits", 3, iniPath);
        if (g.digits < 1) g.digits = 1;
        if (g.digits > 5) g.digits = 5;
        g.digitPanel = GetPrivateProfileIntA(sec, "DigitPanel", -1, iniPath);
        if (g.digitPanel + g.digits > 256) g.digitPanel = -1;
    }
    s_lvNominal = IniFloat("LineVoltage", "Nominal", 750.0f, iniPath);
    s_lvDropPerA = IniFloat("LineVoltage", "DropPerAmp", 0.05f, iniPath);
    s_lvRisePerA = IniFloat("LineVoltage", "RisePerAmp", 0.03f, iniPath);
    s_lvMin = IniFloat("LineVoltage", "Min", 650.0f, iniPath);
    s_lvMax = IniFloat("LineVoltage", "Max", 800.0f, iniPath);
    s_lvFluct.amp = IniFloat("LineVoltage", "Fluctuation", 10.0f, iniPath);
    if (s_lvFluct.amp < 0.0f) s_lvFluct.amp = 0.0f;
    s_lvFluct.period = IniFloat("LineVoltage", "FluctuationPeriod", 3.0f, iniPath);
    if (s_lvFluct.period < 0.1f) s_lvFluct.period = 0.1f;
    s_subRise = IniFloat("LineVoltage", "SubstationRise", 30.0f, iniPath);
    s_subRange = IniFloat("LineVoltage", "SubstationRange", 2000.0f, iniPath);
    if (s_subRange < 1.0f) s_subRange = 1.0f;
    // 制御電圧
    s_cvNominal = IniFloat("ControlVoltage", "Nominal", 100.0f, iniPath);
    s_cvMin = IniFloat("ControlVoltage", "Min", 0.0f, iniPath);
    s_cvMax = IniFloat("ControlVoltage", "Max", 150.0f, iniPath);
    s_cvFluct.amp = IniFloat("ControlVoltage", "Fluctuation", 1.0f, iniPath);
    if (s_cvFluct.amp < 0.0f) s_cvFluct.amp = 0.0f;
    s_cvFluct.period = IniFloat("ControlVoltage", "FluctuationPeriod", 5.0f, iniPath);
    if (s_cvFluct.period < 0.1f) s_cvFluct.period = 0.1f;
}
// ゆらぎを進める: ときどき新しい目標値を選び、そこへゆっくり近づける
static void UpdateFluctuation(Fluctuation& f, int nowMs) {
    if (f.amp <= 0.0f) { f.now = 0.0f; return; }
    if (f.lastMs < 0 || nowMs < f.lastMs) {   // 開始・駅ジャンプで時刻が戻ったとき
        f.lastMs = nowMs;
        f.nextMs = nowMs;
    }
    float dt = (nowMs - f.lastMs) / 1000.0f;
    f.lastMs = nowMs;
    if (nowMs >= f.nextMs) {
        float r = (float)rand() / (float)RAND_MAX;            // 0〜1
        f.target = (r * 2.0f - 1.0f) * f.amp;                 // -amp〜+amp
        float r2 = (float)rand() / (float)RAND_MAX;
        f.nextMs = nowMs + (int)(f.period * (0.5f + r2) * 1000.0f); // 平均の0.5〜1.5倍
    }
    // 目標へ近づける(およそ1秒で差が1/3になる速さ)
    float k = dt * 1.1f;
    if (k > 1.0f) k = 1.0f;
    f.now += (f.target - f.now) * k;
}
// 架線電圧の計算(Min/Max で切る前の値)。計器の表示と、回生失効の判定に使う
static float LineVoltageRaw(const ATS_VEHICLESTATE& vs) {
    float a = vs.Current;
    // 変電所からの距離: 近いほど電圧が高く、電流による上下が小さい(変電所を知らないときは今までどおり)
    float base = s_lvNominal;
    float loadScale = 1.0f;
    if (s_subCount > 0) {
        float d = 1.0e9f;
        for (int s = 0; s < s_subCount; s++) {
            float dd = fabsf((float)vs.Location - s_subPos[s]);
            if (dd < d) d = dd;
        }
        // ※ 変数名に far は使えない(windows.h で別の意味に決められている)
        float distRatio = d / s_subRange;        // 0: 変電所の真横 〜 1: 効きが届かない
        if (distRatio > 1.0f) distRatio = 1.0f;
        base += s_subRise * (1.0f - distRatio);
        loadScale = 0.5f + distRatio;            // 真横で0.5倍 〜 遠いと1.5倍
    }
    float v = (a >= 0.0f) ? base - a * s_lvDropPerA * loadScale : base - a * s_lvRisePerA * loadScale;
    return v + s_lvFluct.now;   // ゆらぎ
}
static void OutputGauges(const ATS_VEHICLESTATE& vs, int* panel) {
    UpdateFluctuation(s_lvFluct, vs.Time);
    UpdateFluctuation(s_cvFluct, vs.Time);
    for (int i = 0; i < GAUGE_MAX; i++) {
        const GaugeSetting& g = s_gauges[i];
        if (g.source == 0) continue;
        float value = 0.0f;
        // 圧力の単位: atsplugin.h には Pa と書いてあるが、BVE5 は kPa で渡す。
        //   念のため、MR圧が 10000 を超えていれば Pa とみなして kPa に直す
        float pScale = (vs.MrPressure > 10000.0f) ? 0.001f : 1.0f;
        switch (g.source) {
        case 1: value = vs.BcPressure * pScale; break;
        case 2: value = vs.MrPressure * pScale; break;
        case 3: value = vs.ErPressure * pScale; break;
        case 4: value = vs.BpPressure * pScale; break;
        case 5: value = vs.SapPressure * pScale; break;
        case 6: value = g_powerOutage ? 0.0f : fabsf(vs.Current); break;   // 回生(マイナス)も大きさで表示。停電中は0A
        case 7: {
            value = LineVoltageRaw(vs);
            if (value < s_lvMin) value = s_lvMin;
            if (value > s_lvMax) value = s_lvMax;
            if (g_powerOutage) value = 0.0f;   // 停電中は0V(Min より下でも0にする)
            break;
        }
        case 8:   // 制御電圧
            value = s_cvNominal + s_cvFluct.now;
            if (value < s_cvMin) value = s_cvMin;
            if (value > s_cvMax) value = s_cvMax;
            break;
        }
        // バー
        if (g.barPanel >= 0) {
            float r = (value - g.minValue) / (g.maxValue - g.minValue);
            if (r < 0.0f) r = 0.0f;
            if (r > 1.0f) r = 1.0f;
            int p = (int)(r * g.steps + 0.5f);   // 段の位置(0〜Steps)
            if (g.segments <= 1) {
                panel[g.barPanel] = p;
            }
            else {
                // 縦に分けた画像ごとのコマ番号(GaugeImages\MakeGauge.ps1 と同じ計算)
                //   0: この画像より下にマーカー(空)、最後: この画像より上にマーカー(満)、その間: 1 + (p - 見える範囲の下端)
                //   つなぎ目の横線を防ぐため、となりの画像と2行ずつ重ね、外側の端は1行広げる
                //   (BVEは画像を拡大するとき、コマの端の行に、となりのコマの行の色を混ぜるため)
                int h = g.markerHalf;
                const int OVERLAP = 2;
                for (int j = 0; j < g.segments; j++) {
                    int a = j * g.steps / g.segments;
                    int b = (j + 1) * g.steps / g.segments;
                    int lo = (j == 0) ? -h - 1 : a - OVERLAP;                          // この画像の絵の一番下の行
                    int hi = (j == g.segments - 1) ? g.steps + h + 1 : b - 1 + OVERLAP; // この画像の絵の一番上の行
                    int frame;
                    if (p < lo - h) frame = 0;
                    else if (p > hi + h) frame = (hi - lo) + 2 * h + 2;
                    else frame = 1 + (p - (lo - h));
                    panel[g.barPanel + j] = frame;
                }
            }
        }
        // 数字
        if (g.digitPanel >= 0) {
            int v = (int)(value + 0.5f);
            if (v < 0) v = 0;
            int div = 1;
            for (int d = 1; d < g.digits; d++) div *= 10;
            bool leading = true;
            for (int d = 0; d < g.digits; d++) {
                int digit = (v / div) % 10;
                bool last = (d == g.digits - 1);
                if (digit != 0 || last) leading = false;
                panel[g.digitPanel + d] = leading ? 0 : digit + 1;
                div /= 10;
            }
        }
    }
}

static void LoadSettings()
{
    {
		srand((unsigned int)time(NULL)); // 乱数の初期化)
        char iniPath[MAX_PATH];
        GetModuleFileNameA(g_hModule, iniPath, MAX_PATH);
        char* lastSlash = strrchr(iniPath, '\\');
        if (lastSlash != NULL) {
            *lastSlash = '\0';
            lstrcatA(iniPath, "\\ATS.ini");
        }
        else {
            lstrcpyA(iniPath, ".\\ATS.ini");
        }
        
        // 一旦、一時的な変数にINIの値を読み出す
        int tempMaxBrake = GetPrivateProfileIntA("VehicleSpec", "MaxBrake", 6, iniPath);

        // 【安全対策】読み込んだ値が 0 以下（負の値など）の場合は、不正データとして無視しデフォルトの 6 を適用する
        if (tempMaxBrake <= 0) {
            g_maxBrakeNotch = 6;
        }
        // 【安全対策】読み込んだ値が 6 より大きい場合は、最大段数を 6 に制限する
        else {
            g_maxBrakeNotch = tempMaxBrake;
        }
        // 【新規】車両のATC搭載仕様を読み込み（0=WS固定, 1=CS固定, 2=両方、デフォルトは両方）
        g_atcTypeSetting = GetPrivateProfileIntA("VehicleSpec", "AtcType", 2, iniPath);
        if (g_atcTypeSetting < 0 || g_atcTypeSetting > 2) g_atcTypeSetting = 2;
        // 車両が対応するTASC/ATO(書かなければ両方とも使える)
        g_tascEnabled = ReadIniBool("VehicleSpec", "TascEnable", true, iniPath);
        g_atoEnabled = ReadIniBool("VehicleSpec", "AtoEnable", true, iniPath);
        // ホームドア(可動柵)連動パネル
        g_psdEnable = ReadIniBool("PlatformDoor", "Enable", true, iniPath);
        g_psdDefault = ReadIniBool("PlatformDoor", "DefaultStation", false, iniPath);
        g_psdPanelOpen = GetPrivateProfileIntA("PlatformDoor", "PanelOpen", 40, iniPath);
        if (g_psdPanelOpen < 0 || g_psdPanelOpen > 255) g_psdPanelOpen = 40;
        g_psdStationHas = g_psdDefault;
        {
            char b[64];
            GetPrivateProfileStringA("PlatformDoor", "OpenDelay", "0", b, sizeof(b), iniPath);  g_psdOpenDelay = (float)atof(b);
            GetPrivateProfileStringA("PlatformDoor", "OpenTime", "3", b, sizeof(b), iniPath);   g_psdOpenTime = (float)atof(b);
            GetPrivateProfileStringA("PlatformDoor", "CloseDelay", "1", b, sizeof(b), iniPath); g_psdCloseDelay = (float)atof(b);
            GetPrivateProfileStringA("PlatformDoor", "CloseTime", "3", b, sizeof(b), iniPath);  g_psdCloseTime = (float)atof(b);
            if (g_psdOpenDelay < 0.0f) g_psdOpenDelay = 0.0f;
            if (g_psdOpenTime < 0.0f) g_psdOpenTime = 0.0f;
            if (g_psdCloseDelay < 0.0f) g_psdCloseDelay = 0.0f;
            if (g_psdCloseTime < 0.0f) g_psdCloseTime = 0.0f;
        }
        g_psdSoundOpen = GetPrivateProfileIntA("PlatformDoor", "SoundOpen", -1, iniPath);
        if (g_psdSoundOpen > 255) g_psdSoundOpen = -1;
        g_psdSoundClose = GetPrivateProfileIntA("PlatformDoor", "SoundClose", -1, iniPath);
        if (g_psdSoundClose > 255) g_psdSoundClose = -1;
        g_psdPanelState = GetPrivateProfileIntA("PlatformDoor", "PanelState", -1, iniPath);
        if (g_psdPanelState > 255) g_psdPanelState = -1;
        // 分けた画像のメーター(速度計)
        g_splitPanel = GetPrivateProfileIntA("SplitMeter", "Panel", -1, iniPath);
        g_splitParts = GetPrivateProfileIntA("SplitMeter", "Parts", 3, iniPath);
        g_splitPerPart = GetPrivateProfileIntA("SplitMeter", "PerPart", 82, iniPath);
        if (g_splitParts < 1) g_splitParts = 1;
        if (g_splitPerPart < 1) g_splitPerPart = 1;
        if (g_splitPanel + g_splitParts > 256) g_splitPanel = -1;
        // 停電
        g_outageCutRegen = ReadIniBool("Outage", "CutRegen", true, iniPath);
        // 回生失効
        g_regenFailEnable = ReadIniBool("RegenFail", "Enable", true, iniPath);
        {
            char b[64] = { 0 };
            GetPrivateProfileStringA("RegenFail", "Limit", "790", b, sizeof(b), iniPath);
            g_regenFailLimit = (float)atof(b);
        }
        g_regenFailPanel = GetPrivateProfileIntA("RegenFail", "Panel", -1, iniPath);
        if (g_regenFailPanel > 255) g_regenFailPanel = -1;
        // 母線引き通し表示灯
        g_busEnable = ReadIniBool("BusLine", "Enable", true, iniPath);
        g_busPanel = GetPrivateProfileIntA("BusLine", "Panel", 197, iniPath);
        if (g_busPanel < 0 || g_busPanel > 255) g_busPanel = 197;
        {
            char b[64] = { 0 };
            GetPrivateProfileStringA("BusLine", "OnSpeed", "5", b, sizeof(b), iniPath);
            g_busOnSpeed = (float)atof(b);
            if (g_busOnSpeed < 0.0f) g_busOnSpeed = 0.0f;
        }
        {
            char b[512] = { 0 };
            GetPrivateProfileStringA("BusLine", "ShoePositions", "", b, sizeof(b), iniPath);
            StripIniComment(b);
            g_busShoeCount = 0;
            char* p = b;
            while (*p != '\0' && g_busShoeCount < BUS_SHOE_MAX) {
                while (*p == ' ' || *p == ',') p++;
                if (*p == '\0') break;
                g_busShoes[g_busShoeCount++] = (float)atof(p);
                while (*p != '\0' && *p != ',') p++;
            }
        }
        // 現在駅・次駅・キロ程の表示
        g_stnEnable = ReadIniBool("StationDisplay", "Enable", true, iniPath);
        g_stnPanelStart = GetPrivateProfileIntA("StationDisplay", "StationPanelStart", 200, iniPath);
        if (g_stnPanelStart < 0 || g_stnPanelStart + 40 > 256) g_stnPanelStart = 200;
        g_kiloPanelStart = GetPrivateProfileIntA("StationDisplay", "KiloPanelStart", 240, iniPath);
        if (g_kiloPanelStart < 0 || g_kiloPanelStart + 4 > 256) g_kiloPanelStart = 240;
        // パネルの更新間隔・反映遅延
        LoadPanelTiming(iniPath);
        // 縦バー形の計器
        LoadGauges(iniPath);

        // --- 【新規】号線仕様の読み込み ---
        g_minLine = GetPrivateProfileIntA("LineSpec", "MinLine", 1, iniPath);
        g_maxLine = GetPrivateProfileIntA("LineSpec", "MaxLine", 9, iniPath);
        g_defaultLine = GetPrivateProfileIntA("LineSpec", "DefaultLine", 1, iniPath);

        // 範囲チェックとバリデーション
        if (g_minLine < 1) g_minLine = 1;
        if (g_maxLine > 9) g_maxLine = 9;
        if (g_minLine > g_maxLine) g_minLine = g_maxLine;

        // デフォルト号線が範囲外の場合は、最小号線または最大号線に補正
        if (g_defaultLine < g_minLine) g_defaultLine = g_minLine;
        if (g_defaultLine > g_maxLine) g_defaultLine = g_maxLine;

        // 構内モード制限速度の読み込み (デフォルト 25km/h)
        g_yardLimitSpeed = GetPrivateProfileIntA("AtcSpec", "YardLimitSpeed", 25, iniPath);
        if (g_yardLimitSpeed <= 0) g_yardLimitSpeed = 25;

        // --- ATC故障パラメータの読み込み ---
        g_atcFailSetting = ReadIniBool("ATC", "ATCFail", false, iniPath);   // ; 以降のコメントは無視する

        char paramBuf[32] = { 0 };
        GetPrivateProfileStringA("ATC", "FailParameter", "0", paramBuf, sizeof(paramBuf), iniPath);
        g_failParameter = (float)atof(paramBuf);

        // 0の処理・境界値チェック
        if (g_failParameter <= 0.0f) {
            g_atcFailSetting = false;
            g_failParameter = 0.0f;


        }
        else {
            // 0.0000001% (0.000000001) 未満は計算発散回避のためアプリ終了
            if (g_failParameter < 0.000000001f) {
                // エラーメッセージをポップアップ表示
                MessageBoxA(
                    NULL,
                    "ATS.ini の [ATC] FailParameter 設定値が無効です。\n"
                    "値が小さすぎるためプラグインを読み込めません。(許容値: 0.000000001 以上)\n\n"
                    "設定値を確認してください。",
                    "エラー",
                    MB_OK | MB_ICONERROR | MB_TOPMOST
                );
                ExitProcess(1); // メッセージ確認後にアプリケーションを終了
            }
            // 1.0を超えている場合は1.0未満に丸める
            if (g_failParameter > 1.0f) {
                g_failParameter = 1.0f;
                MessageBoxA(
                    NULL,
                    "ATS.ini の [ATC] FailParameter 設定値が無効です。\n"
                    "値が大きすぎます。(許容値: 1.00 以下)\n\n"
                    "設定値を確認してください。OKをクリックすると100%に読み替えて続行します。",
                    "警告",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST
                );

            }
        }

        g_extendedNotches = GetPrivateProfileIntA("TASC", "ExtendedNotches", 0, iniPath);
        // 3. INIから PressureRates カンマ区切り文字列を取得
        char pressureRatesBuf[1024] = { 0 };
        GetPrivateProfileStringA("TASC", "PressureRates", "", pressureRatesBuf, sizeof(pressureRatesBuf), iniPath);        // 3. 境界外チェック (ExtendedNotches が PressureRates を上回っている場合)
        StripIniComment(pressureRatesBuf);   // コメントの中のカンマを数えないように、; 以降を取り除く

        // 4. カンマの数から PressureRates 配列の要素数を算出
        int pressureRatesCount = CountCommaSeparatedElements(pressureRatesBuf);
        if (g_extendedNotches > pressureRatesCount) {

            const char* errorMessage =
                "ExtendedNotchesの値がPressureRatesより上回っているため、"
                "インデックスが配列の境界外です。"
                "シナリオを続けることはできません。"
                "アプリケーションを終了します。";

            // 最前面にエラーダイアログを表示
            MessageBoxA(
                NULL,
                errorMessage,
                "TASC/ATC プラグイン エラー",
                MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
            );

            // アプリケーション（BVE本体）を即座に安全終了させる
            ExitProcess(1);
        }
        char tolBuf[32] = { 0 };
        GetPrivateProfileStringA("TASC", "DefaultClearance", "0.5", tolBuf, sizeof(tolBuf), iniPath);
        g_defaultClearance = (float)atof(tolBuf);
        if (g_defaultClearance <= 0.0f) {
            g_defaultClearance = 0.5f; // 負の値は無効として0.5に丸める
        }
        // --TASC故障関連パラメータの読み込み--
        char failurebuf[32] = { 0 };
        g_TASC_FailureActive = ReadIniBool("TASC", "Failure", false, iniPath);   // ; 以降のコメントは無視する
        GetPrivateProfileStringA("TASC", "FailureRate", "0.0", failurebuf, sizeof(failurebuf), iniPath);
        g_TASC_FailureRate = (float)atof(failurebuf);
        if (g_TASC_FailureRate <= 0.0f) { // 0.0未満は無効として0.0に丸める
            g_TASC_FailureActive = false; // 故障率が無効な場合は故障を無効化
            g_TASC_FailureRate = 0.0f; // 故障率を0.0にset
        }
        else {
            if (g_TASC_FailureRate < 0.0000001f) { // 0.0000001% (0.000000001) 未満は計算発散回避のためアプリ終了
                MessageBoxA(
                    NULL,
                    "ATS.ini の [TASC] FailureRate 設定値が無効です。\n"
                    "値が小さすぎるためプラグインを読み込めません。(許容値: 0.0000001 以上)\n\n"
                    "設定値を確認してください。",
                    "エラー",
                    MB_OK | MB_ICONERROR | MB_TOPMOST
                );
                ExitProcess(1);
            }
            if (g_TASC_FailureRate > 1.0f) { // 1.0を超えている場合は1.0未満に丸める
                g_TASC_FailureRate = 1.0f;
                MessageBoxA(
                    NULL,
                    "ATS.ini の [TASC] FailureRate 設定値が無効です。\n"
                    "値が大きすぎます。(許容値: 1.00 以下)\n\n"
                    "設定値を確認してください。OKをクリックすると100%に読み替えて続行します。",
                    "警告",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST
                );
            }

        }
        // --ATO故障関連パラメータの読み込み-- 
        char ATOfailurebuf[32] = { 0 };
        g_ATO_FailureActive = ReadIniBool("ATO", "Failure", false, iniPath);   // ; 以降のコメントは無視する
        GetPrivateProfileStringA("ATO", "FailureRate", "0.0", ATOfailurebuf, sizeof(ATOfailurebuf), iniPath);
        g_ATO_FailureRate = (float)atof(ATOfailurebuf);
        if (g_ATO_FailureRate <= 0.0f) { // 0.0未満は無効として0.0に丸める
            g_ATO_FailureActive = false; // 故障率が無効な場合は故障を無効化
            g_ATO_FailureRate = 0.0f; // 故障率を0.0にset
        }
        else {
            if (g_ATO_FailureRate < 0.0000001f) { // 0.0000001% (0.000000001) 未満は計算発散回避のためアプリ終了
                MessageBoxA(
                    NULL,
                    "ATS.ini の [ATO] FailureRate 設定値が無効です。\n"
                    "値が小さすぎるためプラグインを読み込めません。(許容値: 0.0000001 以上)\n\n"
                    "設定値を確認してください。",
                    "エラー",
                    MB_OK | MB_ICONERROR | MB_TOPMOST
                );
                ExitProcess(1);
            }
            if (g_ATO_FailureRate > 1.0f) { // 1.0を超えている場合は1.OTHに丸める
                g_ATO_FailureRate = 1.0f;
                MessageBoxA(
                    NULL,
                    "ATS.ini の [ATO] FailureRate 設定値が無効です。\n"
                    "値が大きすぎます。(許容値: 1.0 以下)\n\n"
                    "設定値を確認してください。OKをクリックすると100%に読み替えて続行します。",
                    "警告",
                    MB_OK | MB_ICONWARNING | MB_TOPMOST
                );
            }

        }
		ATOLoadSettings(iniPath); //ATO設定の読み込み
		TascLoadSettings(iniPath); //TASC設定の読み込み
        // TASCの目標減速度 (INIでは km/h/s、内部では m/s^2 に換算して使う)
        char decBuf[32] = { 0 };
        GetPrivateProfileStringA("TASC", "Deceleration", "2.5", decBuf, sizeof(decBuf), iniPath);
        float decKmhps = (float)atof(decBuf);
        if (decKmhps < 0.5f || decKmhps > 5.0f) decKmhps = 2.5f;   // 範囲外は既定値
        TASC_DECEL_MS2 = decKmhps / 3.6f;
    }

}

    


// プラグインのバージョンを返す関数 return 値は ATS_VERSION を返す それ以外は無効 ビルドエラーになる可能性があるので、必ず ATS_VERSION を返すこと
ATS_API int WINAPI GetPluginVersion()
{
	return ATS_VERSION; //0x00020000を返す
}

// プラグインがロードされるとき
ATS_API void WINAPI Load()
{
	LoadSettings(); // 設定の読み込み
	InitATO(); // ATO初期化
	ResetGrade(); // こう配補正の初期化
    
    g_currentAtcSpeed = 0; // 現在の閉塞から取得したATC制限速度 (km/h) 初期化
    g_lastRawSignal = 0; // 信号インデックス 初期化
    g_wsLimitSpeed = 0; // WS-ATC制限速度 初期化
    g_csLimitSpeed = 0; // CS-ATC制限速度 初期化
    g_lastCsLimitSpeed = -1; //CS-ATCの前回の制限速度を記憶する変数 初期化
    g_driverPower = 0; // 運転士の現在のハンドル位置を保持する変数 初期化
    g_driverBrake = 0; // 運転士の現在のハンドル位置を保持する変数 初期化
    g_driverReverser = 0; // 運転士の現在のハンドル位置を保持する変数 初期化
    g_isOverrideMode = false; // オーバーライドモード 初期化
    g_isAtcPowerOn = false; // ATCの起動状態 初期化
    g_isBuzzerPlaying = false; // ブザー鳴動中かどうかを記憶するフラグ 初期化
	g_shouldPlayBell = false; // ベルを鳴らす指示を伝えるフラグ 初期化
	g_isOverrideWarningPlaying = false; // 非設モード警報音が鳴動中かどうかを記憶するフラグ 初期化
	g_isYardMode = false; // 構内モード 初期化
    g_isEmergencyOperationMode = false;  //非常運転モード
	g_TASCATOSet = false; //TASC/ATOの有効化 0で手動 INIファイル対応　ATC電源がオフの時は非表示。不具合回避のため、オンにするときは手動をセットする。
	g_ATOPower = 0; //ATO力行指令　0で運転士のノッチと同じ　INIファイル対応　ATC電源がオフの時は非表示
	g_Position = 0; //debug--停車位置までの距離 cm単位で1桁ずつ分ける 999.99mまで。これ以上は不要　ATC電源がオフの時は非表示
    g_PositionEnable = false; //停車位置の認識 手動でもtrueにする。ATC電源がオフの時は非表示
    g_TASCBrakeNotch = 0; //TASC制御・ATO制動指令で使用するブレーキノッチ　0で運転士のノッチと同じ INIファイル対応　ATC電源がオフの時は非表示
    g_TASCControl = false; //TASC制御中かどうか　電源オフの時は非表示
    g_inching = false; //オーバーランしたときにインチングします。（インチングはキー押下のみ手動）
	g_currentLocation = 0.0f; // 現在位置 (m) 初期化
	g_location = 0.0f; // 現在位置 (m) 初期化 --- IGNORE ---
	g_BrakeNotch = 0; // 運転士が現在セットしているブレーキ段数 初期化
	g_driverConstantSpeed = 0; // 運転士が現在セットしている定速段数 初期化
    g_MaxBrakeCNotch = 0; // 常用最大ブレーキ段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
	g_maxPowerNotch = 0; // 最大力行段数（iniから読み込み、0〜6の範囲で設定可能、初期値は6）
    g_atsReverserNotch = 0;
    g_atsBrakeNotch = 0;
    g_brake67Notch = 0;
    g_cars = 0;
	g_pressureRatesCount = 0;
	g_TASC_Failure = false; //TASC故障フラグ　trueで故障
    g_ATO_Failure = false;
    g_isAtcServiceBrake = false;
    g_isAtcEmergencyBrake = false;
    g_speed = 0.0f;

    // 現在選択されているアクティブなATCモード (false = WS-ATC, true = CS-ATC) 初期化
    if (g_atcTypeSetting == 1) {
        g_isCurrentModeCS = true;
    }
    else {
        g_isCurrentModeCS = false;
    }
    g_lastLocation = -1.0f; // 初期位置判定用にリセット
    // 号線を初期値にリセット
    g_currentLine = g_defaultLine; // 中央線初期化
    g_isLineMismatch = false; // 初期化

    // 前方予告作動中フラグを初期化
    g_isAdvanceNoticeActive = false;
    // 号線変更に伴う内部フラグの更新
    UpdateLineFlagsFromCurrentLine();
    g_isInNagataStation = false;
    g_isKeihan = false;
    g_isAtcServiceBrake = false;   // ATC常用パターン接近・介入
    g_isAtcEmergencyBrake = false; // ATC非常ブレーキ作動（02信号）

}
// プラグインが解放される時
ATS_API void WINAPI Dispose()
{
}

// 車両の仕様が設定されるときに呼ばれる関数 通常では使用しない
ATS_API void WINAPI SetVehicleSpec(ATS_VEHICLESPEC vehicleSpec)
{
	g_maxPowerNotch = vehicleSpec.PowerNotches;   // 運転士の最大力行
}
// 運転士がパワーノッチを動かしたとき
ATS_API void WINAPI SetPower(int pos)
{
    g_driverPower = pos;
}

// 運転士がブレーキノッチを動かしたとき
ATS_API void WINAPI SetBrake(int pos)
{
    g_driverBrake = pos;
}

// 運転士がリバーサーを動かしたとき
ATS_API void WINAPI SetReverser(int pos)
{
    g_driverReverser = pos;
}
// BVEから現在の閉塞信号の状態が送られてくる関数
ATS_API void WINAPI SetSignal(int signal)
{
	g_receivedSignal = signal; // 受信した信号インデックスを記録

    // --- 【修正】近鉄けいはんな線(6〜9)とメトロWS(0〜5)のフィルタリング処理 ---
    if (g_isKeihan) {
        // 近鉄モード時：メトロWS専用(1〜5)は無視。絶対停止(0)およびけいはんな線用(6〜9)を受信
        if (signal >= 1 && signal <= 5) {
            return; // 異路線信号を無視（前回の速度を維持）
        }
    }
    else {
        // メトロモード時：近鉄専用(6〜9)は無視
        if (signal >= 6 && signal <= 9) {
            return; // 異路線信号を無視
        }
    }
    // CS-ATCモード運用中にWS信号(0〜9)を受信した場合は完全に無視して旧状態を保持
    if (g_isCurrentModeCS) {
        if (signal < 10) {
            return;
        }
    }
    // WS-ATCモード運用中にCS信号(10以降)を受信した場合も無視して、直前のWS信号を保持する(WS信号を優先)
    //   WS信号をまだ受けていない(直前がCS信号・未受信)ときは、WSの信号がないので停止(0km/h)扱いにする
    else if (signal >= 10) {
        if (g_lastRawSignal < 0 || g_lastRawSignal >= 10) {
            g_lastRawSignal = 0;
            g_wsLimitSpeed = 0;
            g_isAdvanceNoticeActive = false;
        }
        return;
    }
    if (signal != g_lastRawSignal) {
		g_isAdvanceNoticeActive = false; // 信号が変化したら前方予告フラグをリセット
    }
	g_lastRawSignal = signal; // 受信した信号インデックスを記録

    if (signal >= 10) {
        // --- CS-ATC 故障抽選ロジック ---
        if (g_atcFailSetting && !g_isAtcFailed && g_isAtcPowerOn && g_isCurrentModeCS) {
            float rnd = (float)rand() / (float)RAND_MAX;
            if (rnd < g_failParameter) {
                g_isAtcFailed = true; // 故障発生（次閉塞に進んでも自動復帰しない）
            }
        }

        // --- CS-ATC 信号インデックス (10〜24, 43等) ---
        g_csLimitSpeed = GetCsAtcSpeed(signal);

		// --- CS-ATC 制限速度変化時のベル鳴動判定 ---
        if (g_isCurrentModeCS && g_isAtcPowerOn && g_lastCsLimitSpeed >= 0 && !g_isAtcFailed) {
            if (g_csLimitSpeed != g_lastCsLimitSpeed) {
                g_shouldPlayBell = true;
            }
        }
        g_lastCsLimitSpeed = g_csLimitSpeed;
    }
    else {
        // --- WS-ATC 信号インデックス (0～9) ---
        switch (signal) {
        case 0:  g_wsLimitSpeed = 0;  break;
        case 1:  g_wsLimitSpeed = 25; break;
        case 2:  g_wsLimitSpeed = 40; break;
        case 3:  g_wsLimitSpeed = 50; break;
        case 4:  g_wsLimitSpeed = 70; break;
        case 5:  g_wsLimitSpeed = 15; break;
        case 6:  g_wsLimitSpeed = 25; break;
        case 7:  g_wsLimitSpeed = 40; break;
        case 8:  g_wsLimitSpeed = 70; break;
        case 9:  g_wsLimitSpeed = 95; break;
        default:
            // 0〜9以外の未知のインデックスが送られてきた場合のみ安全のため0km/hにする
            g_wsLimitSpeed = 0;
            break;
        }
    }
}

// 地上子を受信したときに呼ばれる関数
ATS_API void WINAPI SetBeaconData(ATS_BEACONDATA beaconData)
{
    // 号線・社線判定地上子 (Type = 20) を受信した場合
    if (beaconData.Type == BEACON_TYPE_LINE_CHECK) {
        int beaconValue = beaconData.Optional;
        bool isMatched = false;

        // A. 北大阪急行 関連地上子データ (1000〜1999)
        if (beaconValue >= 1000 && beaconValue < 2000) {
            g_currentRouteCode = beaconValue;
            // 御堂筋線(1号線)または北急設定(9号線)であれば照合適合
            if (g_currentLine == 1 || g_currentLine == 9) {
                isMatched = true;
            }
            else {
                g_isLineMismatch = false;
            }

        }
        // 長田駅・近鉄直通地上子（4000〜4999）受領時、長田駅エリアと判定
        if (beaconValue >= 4000 && beaconValue < 5000) {
            g_isInNagataStation = true;
        }
        else {
            g_isInNagataStation = false;
        }
        // C. 単一号線データの場合 (0〜10)
        if (beaconValue >= 1 && beaconValue <= 10) {
            if (beaconValue == g_currentLine) {
                isMatched = true;
            }
            else {
                g_isLineMismatch = false;
            }
        }

        // D. 複数号線データの判定 
        else if (beaconValue > 8) {
			// 数値桁解析を行い、各桁の値と現在号線を照合する
			int temp = beaconValue;
			while (temp > 0) {

				if (temp % 100 == g_currentLine) {
					isMatched = true;
					break; // 照合成功したらループを抜ける
				}
                if (temp % 10 == g_currentLine) {
                    isMatched = true;
                    break;
                }
				temp /= 10; // 次の桁に移動
			}
        }

        // 照合結果の判定
        if (isMatched) {
            g_isLineMismatch = false; // 適合：不一致状態を解除
        }
        else {
            g_isLineMismatch = true;  // 不適合または範囲外：非常ブレーキ固縛フラグを立てる
        }

    }
    
    // Type 1030 / 1031 の場合のみ処理を行う（他タイプの地上子による誤作動防止）
    if (beaconData.Type == BEACON_TASC_POSITION || beaconData.Type == BEACON_TASC_POSITIONRANGE) {
        ProcessTascBeacon(beaconData.Type, beaconData.Optional);
    
    }
	// BEACON_TYPE_ADVANCE_NOTICE 信号現示が下がる前の前方予告地上子を受信した場合、フラグを立てる
	if (beaconData.Type == BEACON_TYPE_ADVANCE_NOTICE) {
		if (g_isAtcPowerOn && g_isCurrentModeCS) {
			ATOOnAdvanceNotice(GetCsAtcSpeed(beaconData.Signal), beaconData.Distance, g_lastLocation);
			if (beaconData.Signal < g_lastRawSignal) {
				g_isAdvanceNoticeActive = true; // 前方予告作動中フラグを立てる
			}
			else {
				g_isAdvanceNoticeActive = false; // 信号に変化ない時前方予告解除フラグを立てる
			}
		}
	}
	// ATO自動運転開始地上子を受信した場合、ATO自動運転を開始する
	if (beaconData.Type == BEACON_ATOAUTOSTART) {
		ATOOnAutoStartBeacon();
	}
	// ホームドア(可動柵)の有無を受信した場合(次に止まる駅の設定)
	if (beaconData.Type == BEACON_PLATFORMDOOR) {
		g_psdStationHas = (beaconData.Optional != 0);
	}
	// 始発駅・駅ジャンプのあとの定点停止灯(停車位置に置く。Optional: 許容範囲cm、0は既定値)
	if (beaconData.Type == BEACON_STATIONLAMP) {
		TascOnStationLampBeacon(beaconData.Optional);
	}
	// 停電区間 (Optional 1: ここから停電、0: ここで復旧)
	if (beaconData.Type == BEACON_OUTAGE) {
		g_powerOutage = (beaconData.Optional != 0);
	}
	// 変電所の位置 (Optional: 変電所の距離程(m))。位置を直接書くので、どこで受けてもよい
	if (beaconData.Type == 1045 && beaconData.Optional > 0) {
		AddSubstation((float)beaconData.Optional);
	}
	// サードレールのない区間 (Optional: この地上子からの長さcm)
	//   駅ジャンプの直後に止まったまま届いたものは、通過したものではないので使わない
	if (beaconData.Type == BEACON_NORAIL && beaconData.Optional > 0 && fabsf(g_speed) >= 0.1f
		&& g_busGapCount < BUS_GAP_MAX && g_lastLocation >= 0.0f) {
		g_busGapStart[g_busGapCount] = g_lastLocation;
		g_busGapEnd[g_busGapCount] = g_lastLocation + beaconData.Optional / 100.0f;
		g_busGapCount++;
	}
	// 現在駅と次駅 (Optional: 現在駅×1000＋次駅。負の値は上り)
	if (beaconData.Type == BEACON_STATIONINFO) {
		int v = beaconData.Optional;
		g_stnUp = (v < 0);
		if (v < 0) v = -v;
		g_stnCurrent = v / 1000;
		g_stnNext = v % 1000;
	}
	// キロ程 (Optional: キロ程と距離程の差(m))。地上子の位置に関係なく、距離程から計算する
	if (beaconData.Type == BEACON_KILOPOST) {
		g_kiloOffset = beaconData.Optional;
		g_kiloValid = true;
	}

	// 速度制限地上子を受信した場合、ATOの速度制限を更新する
    if (beaconData.Type == BEACON_SPEEDLIMIT) {
        // 速度制限地上子を受信した場合、ATOの速度制限を更新する
        ATOOnSpeedLimitBeacon(beaconData.Optional, g_lastLocation);
    }
    // こう配地上子(1008)を受信した場合、TASC/ATOのこう配補正を更新する
    if (beaconData.Type == BEACON_LEVEL) {
        ProcessGradeBeacon(beaconData.Optional, g_lastLocation);
    }
}
// 運転士がキーを押したときの処理
ATS_API void WINAPI KeyDown(int key)
{
    // 加速側がN（0）かつ、ブレーキ側が非常ブレーキ（g_maxBrakeNotch + 1）のときだけ true になるフラグ
    bool isSafetyConditionMet = (g_driverPower == 0 && g_driverBrake == (g_maxBrakeNotch + 1));

    // ATS_KEY_B1キー (Delete): 非設切替 非常ブレーキ掛けた状態である必要あり

    if (key == ATS_KEY_B1) {
        if (isSafetyConditionMet) {
            g_isOverrideMode = !g_isOverrideMode;
            if (g_isOverrideMode) {
                g_isYardMode = false;
                g_isEmergencyOperationMode = false;
            }
        }
    }

    // 非常運転モード 非設モードと構内モードが起動していない状態のみ
    if (key == ATS_KEY_C2) {
        if (isSafetyConditionMet) {
            g_isEmergencyOperationMode = !g_isEmergencyOperationMode;
            if (g_isEmergencyOperationMode) {
                g_isYardMode = false;
                g_isOverrideMode = false;
            }
        }
    }
    // ATS_KEY_A1（Insert キー）：ATC電源のON/OFF
    if (key == ATS_KEY_A1) {
        // 【インターロック】安全条件を満たしているときだけ電源操作を許可
        if (isSafetyConditionMet) {
            g_isAtcPowerOn = !g_isAtcPowerOn;
            // ATC電源をオンにした時は手動(0)をセット
            if (g_isAtcPowerOn) {
                g_TASCATOSet = 0;
                InitTasc();
            }
            else {
                g_PositionEnable = false;
            }
        }
    }

    // ATS_KEY_C1（ pageupキー）：保安装置切り替え
// 【修正】手動モード切替時のCS状態リセット
    if (key == ATS_KEY_C1) {
        if (g_atcTypeSetting == 2 && g_isAtcPowerOn && isSafetyConditionMet && !g_isKeihan) {
            g_isCurrentModeCS = !g_isCurrentModeCS;

            // CSからWSに切り替わった場合はCS保持状態をリセット
            if (!g_isCurrentModeCS) {
                ResetCsAtcState();
            }
            SetSignal(g_receivedSignal);
        }
    }

    // ATS_KEY_B2（End キー）：構内モード切替 使用しない可能性があるためコメントアウトで保留する。
    //if (key == ATS_KEY_B2) {
        //if (g_isAtcPowerOn && isSafetyConditionMet) {
       //     g_isYardMode = !g_isYardMode;
        //    if (g_isYardMode) g_isOverrideMode = false; // 排他制御
   //     }
  //  }
    // ATS_KEY_E (3 キー): 号線切り替え（進む / +1） 近鉄モード時に切り替えないように。
    if (key == ATS_KEY_E) {
        if (g_isAtcPowerOn && isSafetyConditionMet && !g_isKeihan) {
            g_currentLine++;
            if (g_currentLine > g_maxLine) {
                g_currentLine = g_minLine;
            }
            g_isLineMismatch = false;

            UpdateLineFlagsFromCurrentLine();
            SetSignal(g_receivedSignal);
        }
    }
    // ATS_KEY_D (2 キー): 号線戻し (-1)　近鉄モード時は切り替えないように
    if (key == ATS_KEY_D) {
        if (g_isAtcPowerOn && isSafetyConditionMet && !g_isKeihan) {
            g_currentLine--;
            if (g_currentLine < g_minLine) {
                g_currentLine = g_maxLine;
            }
            g_isLineMismatch = false;

            UpdateLineFlagsFromCurrentLine();
            SetSignal(g_receivedSignal);
        }
    }
        // --- 長田駅での近鉄直通モード切替（4キー: ATS_KEY_F） ---
    if (key == ATS_KEY_F) { // 4キー
        if (g_isAtcPowerOn && isSafetyConditionMet && g_isInNagataStation && !g_isCurrentModeCS) {
            if (!g_isKeihan && g_currentLine == 4) {
                // 中央線モード -> 近鉄けいはんな線モード
                g_isKeihan = true;
                g_isChuo = false;
                ResetCsAtcState(); // 近鉄線（WS）切替に伴うリセット
            }
            else if (g_isKeihan)
               {
                // 近鉄けいはんな線モード -> 中央線モード復帰
                g_isKeihan = false;
                g_isChuo = true;
                g_isCurrentModeCS = (g_atcTypeSetting == 1 || g_atcTypeSetting == 2);
            }
            SetSignal(g_receivedSignal);
        }
    }
    // =========================================================
    // TASC/ATO モード切替キー (ATS_KEY_C2)
    // =========================================================
    if (key == ATS_KEY_G) {
        // ATC電源がONかつ安全条件を満たしている場合のみ切替を許可
        if (g_isAtcPowerOn && isSafetyConditionMet) {
            // 0: 手動 -> 1: TASC -> 2: ATO -> 0: 手動... と循環
            // INIで使えないようにしたモードは飛ばす(手動は必ず使える)
            do {
                g_TASCATOSet++;
                if (g_TASCATOSet > 2) {
                    g_TASCATOSet = 0;
                }
            } while ((g_TASCATOSet == 1 && !g_tascEnabled) || (g_TASCATOSet == 2 && !g_atoEnabled));
            
            // モード切替時にTASCの内部状態をリセット
            InitTasc();
            TascArmHold();   // 止まっていれば転動防止ブレーキを掛ける(TASC/ATOモードのとき)
        }
    }
    if (key == ATS_KEY_H) { ATOOnStartKey();}
	if (key == ATS_KEY_I) {	g_inching = true;} //インチングキー}

}

// Elapse関数: 1フレームごとの処理を行う関数
ATS_API ATS_HANDLES WINAPI Elapse(ATS_VEHICLESTATE vehicleState, int* panel, int* sound)
{
    ATS_HANDLES handles = {};
    // 1. 現在位置と速度を明示的に安全な型へキャストして取得
    float currentLocation = (float)vehicleState.Location; // m単位
    float currentSpeed = (float)vehicleState.Speed;       // km/h単位
    int currentTimeMs = vehicleState.Time; // ★ 時刻(ms)を取得
	g_speed = vehicleState.Speed; // 現在速度

    // 現在の位置を保持
    g_lastLocation = vehicleState.Location;
    TascTrackStartLamp(vehicleState.Location, vehicleState.Speed); // 地上子1041の位置から動いたら取り消す	// --- 0. 運転士のハンドル位置を一時変数にコピー ---
    int outputPower = g_driverPower;
    int outputBrake = g_driverBrake;
    // 【重要】現在選択されているアクティブなモードに応じて、採用する制限速度をパチッと切り替える
    g_currentAtcSpeed = g_isCurrentModeCS ? g_csLimitSpeed : g_wsLimitSpeed;
    // --- 1. ATC電源状態の判定 ---
    if (!g_isAtcPowerOn) {
        outputBrake = g_maxBrakeNotch + 1;
        outputPower = 0;
        g_isOverrideMode = false;
        g_isAdvanceNoticeActive = false;
        g_isAtcEmergencyBrake = false;
        g_isAtcServiceBrake = false;
        g_isEmergencyOperationMode = false;


        if (g_isOverrideWarningPlaying) {
            sound[2] = ATS_SOUND_STOP;
            g_isOverrideWarningPlaying = false;
        }
        if (g_isBuzzerPlaying ) {
            sound[0] = ATS_SOUND_STOP;
            g_isBuzzerPlaying = false;
        }
    }
    else {
        // --- 2. 通常のWS-ATC自動ブレーキ制御の判定ロジック ---
        g_isAtcEmergencyBrake = false;   // ★追加: 毎フレーム最初にクリア
        g_isAtcServiceBrake = false;     // ★追加
        if (g_isLineMismatch) {
			InitTasc(); // TASC/ATOの内部状態をリセット
        }
        int tascBrake = CalculateTascBrake(vehicleState.Location, vehicleState.Speed, g_driverBrake, currentTimeMs); // ★追加: TASC計算はここで1回だけ

        int effectiveLimitSpeed = g_currentAtcSpeed;

        if (g_isYardMode) {
            // 構内モード選択時：本線信号(11〜30)が同時に送られてきていても無視し、構内制限速度(25km/h)を優先
            effectiveLimitSpeed = g_yardLimitSpeed;
        }
        else if (g_currentAtcSpeed == 0 && g_isOverrideMode) {
            effectiveLimitSpeed = 25; // 非設/開放モード (25km/h)
        }
        else if (g_isEmergencyOperationMode) {
            // ATC非常運転（開放）モード：最高速度制限なし（無制限）
            effectiveLimitSpeed = 999;
        }
        // 1. ATC故障時判定
        // 1. 非常運転モードON時（故障中・正常時問わず最優先で緩解・手動運転許可）
        if (g_isEmergencyOperationMode) {
            outputBrake = g_driverBrake;
            outputPower = g_driverPower;
            g_isAtcEmergencyBrake = false;
            g_isAtcServiceBrake = false;
        }
        // 2. ATC故障時判定（非常運転モードOFFの場合）
        else if (g_isAtcFailed) {
            outputBrake = g_maxBrakeNotch + 1;
            outputPower = 0;
            g_isAtcEmergencyBrake = true;
            g_isAtcServiceBrake = false;
        }
        // 3. 非常ブレーキ判定（号線不一致、CS 02信号、WS 0信号時）
        else if (g_isLineMismatch ||
            (g_isCurrentModeCS && g_lastRawSignal == 10 && !g_isOverrideMode) ||
            (!g_isCurrentModeCS && g_lastRawSignal == 0 && !g_isOverrideMode)) {

            outputBrake = g_maxBrakeNotch + 1;
            outputPower = 0;
            g_isAtcEmergencyBrake = true;
            g_isAtcServiceBrake = false;
        }
        // 4. 常用ブレーキ判定（CS-ATC 01信号・速度超過 / WS-ATC 1〜9信号・速度超過）
        else if (!g_isOverrideMode && (
            (g_isCurrentModeCS && ((g_lastRawSignal == 11 || g_lastRawSignal == 12) ||
                (g_lastRawSignal >= 13 && vehicleState.Speed > (float)effectiveLimitSpeed))) ||
            (!g_isCurrentModeCS && g_lastRawSignal >= 1 && g_lastRawSignal <= 9 && vehicleState.Speed > (float)effectiveLimitSpeed))) {

            if (g_driverBrake > g_maxBrakeNotch) {
                outputBrake = g_driverBrake; // 運転士非常ブレーキ操作優先
            }
            else {
                outputBrake = g_maxBrakeNotch; // ATC常用最大ブレーキ
            }

            outputPower = 0;
            g_isAtcEmergencyBrake = false;
            g_isAtcServiceBrake = true;
        }

        if (!g_isAtcEmergencyBrake && !g_isAtcServiceBrake && tascBrake > outputBrake) {
            outputBrake = tascBrake;   // ATCが介入していないときだけ、TASCのブレーキを採用
        }
        

        // 超過ブザー (WS-ATCモード時かつ構内モードOFF時のみ動作)
        // ビー音
        if (!g_isCurrentModeCS && !g_isYardMode && vehicleState.Speed > (float)effectiveLimitSpeed && vehicleState.Speed > 0.0f && !g_isAtcFailed) {
            if (!g_isBuzzerPlaying) {
                sound[0] = ATS_SOUND_PLAYLOOPING;
                g_isBuzzerPlaying = true;
            }
        }
        else {
            if (g_isBuzzerPlaying) {
                sound[0] = ATS_SOUND_STOP;
                g_isBuzzerPlaying = false;
            }
        }

		bool isAtcSignalReceived = ((g_lastRawSignal >= 10 && g_lastRawSignal <= 30) || g_lastRawSignal == 43); // 非設モード警報音の条件判定

        if (g_isOverrideMode && isAtcSignalReceived && !g_isAtcFailed) {
            if (!g_isOverrideWarningPlaying) {
                sound[2] = ATS_SOUND_PLAYLOOPING;
                g_isOverrideWarningPlaying = true;
            }
        }
        else {
            if (g_isOverrideWarningPlaying) {
                sound[2] = ATS_SOUND_STOP;
                g_isOverrideWarningPlaying = false;
            }

        }
    }

	// --- 5. ATO運転処理 ---
	bool atcFree = g_isAtcPowerOn && !g_isAtcEmergencyBrake && !g_isAtcServiceBrake && !g_isOverrideMode && !g_isEmergencyOperationMode;
	ATOUpdate(atcFree, vehicleState.Speed, g_currentAtcSpeed, (float)vehicleState.Location, vehicleState.Time); // ATO更新処理
    outputPower = ATOConvertDriverPower(outputPower);   // 力行の拡張ノッチを使うとき、運転士のノッチをATOのノッチに変換する
    if (g_ATOStart == 1) {
        outputPower = ATOGetPower();        // ATO運転中は力行ノッチをATOが決める(0なら惰行。拡張ノッチ対応)
        int atoBrake = ATOGetBrake();
        if (atoBrake > outputBrake) {       // 運転士・ATC・TASCのブレーキより強いときだけ採用
            outputBrake = atoBrake;
            outputPower = 0;
        }
    }
    // --- 5b. インチング(目標位置の手前・奥で止まったとき、キーを押している間だけ動く) ---
    int outputReverser = g_driverReverser;
    if (TascInchingUpdate(g_inching, atcFree, (float)vehicleState.Location, vehicleState.Speed,
        g_driverBrake, g_driverPower, g_driverReverser)) {
        outputPower = ATOConvertDriverPower(TascInchingPower());   // インチングのノッチも変換する
        int inchBrake = TascInchingBrake();
        outputBrake = (g_driverBrake > inchBrake) ? g_driverBrake : inchBrake;   // 運転士のブレーキが強ければそちらを優先
        outputReverser = TascInchingReverser();
    }

	// --- 6. ハンドル出力の設定 ---
    if (g_powerOutage) {
        outputPower = 0;                              // 停電中は力行しない
        if (g_outageCutRegen) outputReverser = 0;     // 回生ブレーキも使えないので、レバーサーを中立にして電気ブレーキを止める(空気ブレーキだけになる)
    }
    // 回生失効: 回生ブレーキ中(電流がマイナス)に架線電圧が上限を超えたら失効。ブレーキを緩める・力行する・止まるまで続く
    if (g_regenFailEnable && !g_powerOutage) {
        if (!g_regenFail && outputBrake > 0 && vehicleState.Current < 0.0f
            && LineVoltageRaw(vehicleState) > g_regenFailLimit) {
            g_regenFail = true;
        }
        if (g_regenFail && (outputBrake == 0 || outputPower > 0 || fabsf(vehicleState.Speed) < 1.0f)) {
            g_regenFail = false;
        }
        if (g_regenFail) outputReverser = 0;          // 電気ブレーキを止める(空気ブレーキだけになる)
    }
    else {
        g_regenFail = false;
    }
    handles.Power = outputPower;
    handles.Brake = outputBrake;
    handles.Reverser = outputReverser;
    handles.ConstantSpeed = 0;

    // ----------------------------------------------------
    // 7. パネル出力処理 (INI使わず定数値で直接指定)
    // ----------------------------------------------------

    // TASCブレーキ指令 (DigitalNumber): 0=非表示(手動・ATC電源OFF), 1=N, 2以降=B1, B2, …
    //   TASCモードでは、TASCが制御していないときも「N」を表示する(ATOモードではATO指令(139)に表示するので非表示)
    bool showTascPanel = g_isAtcPowerOn && g_TASCATOSet == 1;
    panel[PANEL_TASC_NOTCH] = showTascPanel ? GetCurrentTascBrakeNotch() + 1 : 0;

    // 定点停止灯 (PilotLamp): 故障中は 0 (消灯)
    if (PANEL_POSITION_ENABLE >= 0 && PANEL_POSITION_ENABLE < 256) {
        panel[PANEL_POSITION_ENABLE] = (g_PositionEnable && !g_TASC_Failure) ? 1 : 0;
    }

	panel[1] = g_isOverrideMode ? 1 : 0; // 非設をONにしているかどうかを出力
	panel[2] = g_isAtcPowerOn ? 1 : 0; // ATC電源がONかOFFかを出力
    //panel[3] = g_maxBrakeNotch; // 【デバッグ用】読み取ったブレーキ最大段数を出力
    //panel[4] = g_isCurrentModeCS ? 1 : 0; //現在のモード (0 = WS, 1 = CS)
    panel[5] = g_currentLine; //選択中の号線インデックス (1〜10)
    //panel[6] = g_isLineMismatch ? 1 : 0; // [デバッグ用]号線不一致警告灯（0:正常, 1:異常/固縛）
    //panel[7] = g_isYardMode ? 1 : 0; // 構内モード表示灯 使用しないかもしれません。
    panel[9] = g_isKeihan ? 1 : 0; //近鉄けいはんな線モード
    panel[53] = g_isAtcFailed ? 1 : 0; // ATC故障表示 (1:点灯)
    panel[54] = g_isEmergencyOperationMode ? 1 : 0; // ATC非常運転モード表示 (1:点灯)
	panel[133] = g_TASCATOSet + 1; // TASC/ATOモード切替状態 (0:手動, 1:TASC, 2:ATO)
	panel[134] = (g_TASCATOSet == 1 && IsTascActive()) ? 1 : 0; // TASCモード表示灯 (1:点灯) 

    //panel[11] = g_isAtcServiceBrake ? 1 : 0;   // ATC常用 (0:非表示, 1:点灯)
    //panel[12] = g_isAtcEmergencyBrake ? 1 : 0; // ATC非常 (0:非表示, 1:点灯)
    // CS-ATC車内信号灯パネル（10〜30）および開通表示灯（31, 32）を初期化 (0:消灯)
    for (int i = 10; i <= 32; i++) {
        panel[i] = 0;
    }
    // --- ATC表示灯制御 (0=非表示, 1=WS, 2=CS) ---
    if (g_isKeihan || !g_isAtcPowerOn) {
        // 近鉄けいはんな線モード時、またはATC電源OFF時は「0 (非表示)」
        panel[5] = 0; //号線インデックスの表示をオフ
        panel[4] = 0; // WS-ATC/CS-ATCモード表示をオフ
        panel[11] = 0;
        panel[12] = 0;
        panel[50] = 0;   // ATC常用 (0:非表示, 1:点灯)
        panel[51] = 0; // ATC非常 (0:非表示, 1:点灯)
		if (!g_isAtcPowerOn) panel[133] = 0; // TASC/ATOモード切替状態 (0:手動, 1:TASC, 2:ATO)
		
		panel[134] = 0; // TASCモード表示灯 (1:点灯)
    }
    else {
        // 中央線モード時
        if (g_isCurrentModeCS) {
            panel[4] = 2; // CS-ATCモード時は「2」を出力
        }
        else {
            panel[4] = 1; // WS-ATCモード時は「1」を出力
            panel[50] = 0;   // ATC常用 (0:非表示, 1:点灯)
            panel[51] = 0; // ATC非常 (0:非表示, 1:点灯)

        }
    }
        for (int i = 10; i <= 32; i++) {
        panel[i] = 0;
    }
    // CS-ATCかつ電源ONの時のみ、各種表示灯を出力
    if (g_isAtcPowerOn && g_isCurrentModeCS && !g_isKeihan && !g_isAtcFailed) {
        // 構内モード時は車内信号・開通表示灯・予告灯をすべて消灯 現在未使用
        if (g_isYardMode) {
            panel[0] = 0;

        }
        else {
            panel[0] = g_csLimitSpeed; // デジタル車内信号速度 (数値)
            panel[50] = g_isAtcServiceBrake ? 1 : 0;   // ATC常用 (0:非表示, 1:点灯)
            panel[51] = g_isAtcEmergencyBrake ? 1 : 0; // ATC非常 (0:非表示, 1:点灯)
            if (g_isAdvanceNoticeActive) {
                panel[33] = 1; // 前方予告作動中フラグを点灯
            }
			else {
				panel[33] = 0; // 前方予告作動中フラグを消灯
			}

            if (g_isLineMismatch) {
                panel[0] = 0;
                panel[51] = 1; // ATC非常 (0:非表示, 1:点灯)
                // panel[31], panel[32] は 0 (消灯)
            }
            // 受信しているCS-ATC信号インデックス (10〜30) に応じた処理
            else if (g_lastRawSignal >= 10 && g_lastRawSignal <= 30) {
                // 1. 各信号インデックスに対応する車内信号灯を点灯
                panel[g_lastRawSignal] = 1;

                // 2. 開通状態表示灯の出力設定
                if (g_lastRawSignal >= 13) {
                    // case 13 以上：進行（開通）信号
                    panel[31] = 1; // 進行表示灯
                    panel[32] = 0;
                }
                else if (g_lastRawSignal == 11 || g_lastRawSignal == 12) {
                    // case 11 / 12：停止（01信号）
                    panel[31] = 0;
                    panel[32] = 1; // 停止(01)表示灯
                    panel[50] = 1;   // ATC常用 (0:非表示, 1:点灯)
                    panel[51] = 0; // ATC非常 (0:非表示, 1:点灯)

                }
                else if (g_lastRawSignal == 10) {
                    // case 10：絶対停止・無信号（進入時）
                    panel[31] = 0; // 消灯
                    panel[32] = 0; // 消灯
				    panel[g_lastRawSignal] = 0; // 10番のインデックスは絶対停止なので、車内信号灯は消灯
                    panel[0] = 0;
				    panel[33] = 0; // 前方予告作動中フラグを消灯
                    panel[50] = 0;   // ATC常用 (0:非表示, 1:点灯)
                    panel[51] = g_isAtcEmergencyBrake ? 1 : 0;
                }
            }
        }
    }
    else {
        // WS-ATCモード時または電源OFF時は車内信号指示・表示灯をすべて消灯 (0)
        panel[0] = 0;
        panel[33] = 0; // 前方予告作動中フラグを消灯
        if (g_isAtcFailed) {
            panel[51] = g_isAtcEmergencyBrake ? 1 : 0;
            panel[50] = 0;   // ATC常用 (0:非表示, 1:点灯)

        }
    }
    panel[PANEL_ATO_STATE] = (g_ATOStart < 0) ? 0 : ((g_ATOStart == 0) ? 1 : 2);
    // ATO力行・ブレーキ指令 (DigitalNumber): 0=非表示(ATOモード以外・ATC電源OFF), 1=N, 2以降=P1/B1, P2/B2, …
    //   発進待ちや惰行中も「N」を表示する。駅に止まるときのTASCのブレーキも、ATOのブレーキ指令として表示する
    bool showAtoPanel = g_isAtcPowerOn && g_TASCATOSet == 2;
    int atoBrakeStep = ATOGetBrakeStep();
    int tascBrakeStep = GetCurrentTascBrakeNotch();
    if (tascBrakeStep > atoBrakeStep) atoBrakeStep = tascBrakeStep;
    //   力行とブレーキの画像を同じ場所に置けるよう、どちらか一方だけを表示する
    //   力行中 → 力行指令(138)だけ / それ以外(ブレーキ中・N) → ブレーキ指令(139)だけ(Nもこちらに表示)
    int atoPowerStep = (g_ATOStart == 1) ? g_ATOPower : 0;
    bool atoPowering = (atoPowerStep > 0 && atoBrakeStep == 0);
    panel[PANEL_ATO_POWER] = (showAtoPanel && atoPowering) ? atoPowerStep + 1 : 0;
    panel[PANEL_ATO_BRAKE] = (showAtoPanel && !atoPowering) ? atoBrakeStep + 1 : 0;
    panel[PANEL_ATO_FAILURE] = (g_ATO_Failure && g_isAtcPowerOn && g_TASCATOSet == 2) ? 1 : 0;
    panel[PANEL_ATO_AUTOSTART] = ATOIsAutoStartReady() ? 1 : 0;
    panel[PANEL_INCHING] = TascIsInching() ? 1 : 0;   // インチング中表示灯
    // 可動柵開(ホームドア連動): ホームドアのある駅で、車両のドアが開いている間だけ点灯
    //   INIで Enable = false の車両(非対応)では常に消灯
    // ホームドアの開閉の動き
    {
        bool active = g_psdEnable && g_psdStationHas;
        if (g_psdSoundOpen >= 0) sound[g_psdSoundOpen] = ATS_SOUND_CONTINUE;
        if (g_psdSoundClose >= 0) sound[g_psdSoundClose] = ATS_SOUND_CONTINUE;
        if (g_psdDoorOpenEvent) {
            g_psdDoorOpenEvent = false;
            g_psdWaitClose = false;
            if (active && g_psdInstant) {          // シナリオ開始・駅ジャンプ直後: すでに開いている
                g_psdState = PSD_OPEN;
                g_psdWaitOpen = false;
            }
            else if (active && g_psdState != PSD_OPEN && g_psdState != PSD_OPENING) {
                g_psdWaitOpen = true;
                g_psdStateUntilMs = currentTimeMs + (int)(g_psdOpenDelay * 1000.0f);
            }
        }
        if (g_psdDoorCloseEvent) {
            g_psdDoorCloseEvent = false;
            g_psdWaitOpen = false;
            if (g_psdState == PSD_OPEN || g_psdState == PSD_OPENING) {
                g_psdWaitClose = true;
                g_psdStateUntilMs = currentTimeMs + (int)(g_psdCloseDelay * 1000.0f);
            }
        }
        if (g_psdInstant && fabsf(vehicleState.Speed) > 1.0f) g_psdInstant = false;
        // 駅ジャンプ直後に1040があとから届いたとき: 車両のドアが開いていれば、ホームドアも開いた状態にする
        if (active && g_psdInstant && g_isDoorOpen && g_psdState == PSD_CLOSED && !g_psdWaitOpen) {
            g_psdState = PSD_OPEN;
        }
        if (!active) {                              // ホームドアのない駅・非対応車両: いつも閉
            g_psdState = PSD_CLOSED;
            g_psdWaitOpen = g_psdWaitClose = false;
        }
        else if (g_psdWaitOpen && currentTimeMs >= g_psdStateUntilMs) {
            g_psdWaitOpen = false;
            g_psdState = PSD_OPENING;
            g_psdStateUntilMs = currentTimeMs + (int)(g_psdOpenTime * 1000.0f);
            if (g_psdSoundOpen >= 0) sound[g_psdSoundOpen] = ATS_SOUND_PLAY;
        }
        else if (g_psdWaitClose && currentTimeMs >= g_psdStateUntilMs) {
            g_psdWaitClose = false;
            g_psdState = PSD_CLOSING;
            g_psdStateUntilMs = currentTimeMs + (int)(g_psdCloseTime * 1000.0f);
            if (g_psdSoundClose >= 0) sound[g_psdSoundClose] = ATS_SOUND_PLAY;
        }
        else if (g_psdState == PSD_OPENING && currentTimeMs >= g_psdStateUntilMs) {
            g_psdState = PSD_OPEN;
        }
        else if (g_psdState == PSD_CLOSING && currentTimeMs >= g_psdStateUntilMs) {
            g_psdState = PSD_CLOSED;
        }
        if (currentTimeMs < g_psdStateUntilMs - 600000) g_psdStateUntilMs = currentTimeMs; // 時刻が大きく戻ったとき(駅ジャンプ)
    }
    // 回生失効の表示灯
    if (g_regenFailPanel >= 0) panel[g_regenFailPanel] = g_regenFail ? 1 : 0;
    // 分けた画像のメーター(速度計): 画像 j は 値 j*PerPart 〜 (j+1)*PerPart-1 を受け持つ
    //   値より前の画像は最後のコマ(全部点灯)、値を受け持つ画像はその値のコマ、値より後の画像は空のコマ
    if (g_splitPanel >= 0) {
        int v = (int)fabsf(vehicleState.Speed);   // BVE の kmph と同じく、小数点以下は切り捨て
        for (int j = 0; j < g_splitParts; j++) {
            int first = j * g_splitPerPart;
            int last = first + g_splitPerPart - 1;
            int f;
            if (v < first) f = 0;
            else if (v > last) f = g_splitPerPart;
            else f = v - first + 1;
            panel[g_splitPanel + j] = f;
        }
    }
    // 母線引き通し表示灯: M車の集電靴が1つでもサードレールのない区間に入っていれば消灯
    if (g_busEnable) {
        float front = (float)vehicleState.Location;
        float maxShoe = 0.0f;
        for (int s = 0; s < g_busShoeCount; s++) if (g_busShoes[s] > maxShoe) maxShoe = g_busShoes[s];
        bool off = false;
        for (int k = 0; k < g_busGapCount; k++) {
            if (g_busShoeCount == 0) {           // 集電靴の位置を書いていないときは、先頭の位置で判断する
                if (front >= g_busGapStart[k] && front < g_busGapEnd[k]) off = true;
            }
            for (int s = 0; s < g_busShoeCount; s++) {
                float shoe = front - g_busShoes[s];
                if (shoe >= g_busGapStart[k] && shoe < g_busGapEnd[k]) off = true;
            }
        }
        // 最後尾の集電靴も通り過ぎた区間は消す
        for (int k = 0; k < g_busGapCount; ) {
            if (front - maxShoe >= g_busGapEnd[k]) {
                g_busGapStart[k] = g_busGapStart[g_busGapCount - 1];
                g_busGapEnd[k] = g_busGapEnd[g_busGapCount - 1];
                g_busGapCount--;
            }
            else k++;
        }
        if (fabsf(vehicleState.Speed) < g_busOnSpeed) off = true;   // 低速・停車中は引き通さない
        if (g_powerOutage) off = true;                               // 停電中は消灯
        panel[g_busPanel] = off ? 0 : 1;
    }
    // 可動柵開ランプ: ホームドアが閉まりきっていない間(開いている途中・開・閉まっている途中)点灯
    panel[g_psdPanelOpen] = (g_psdState != PSD_CLOSED) ? 1 : 0;
    if (g_psdPanelState >= 0) panel[g_psdPanelState] = g_psdState;

    // 現在駅・次駅 (号線ごとに4つ: 下り現在・下り次・上り現在・上り次)。選択中の号線と方向だけ駅番号を出し、ほかは0(非表示)
    for (int i = 0; i < 40; i++) panel[g_stnPanelStart + i] = 0;
    if (g_stnEnable && g_currentLine >= 1 && g_currentLine <= 10) {
        int base = g_stnPanelStart + (g_currentLine - 1) * 4 + (g_stnUp ? 2 : 0);
        panel[base] = g_stnCurrent;
        panel[base + 1] = g_stnNext;
    }
    // キロ程 (4桁: 10km・1km・100m・10m)。0=非表示、1以降=数字0〜9。10kmの桁が0のときは非表示
    for (int i = 0; i < 4; i++) panel[g_kiloPanelStart + i] = 0;
    if (g_stnEnable && g_kiloValid) {
        double loc = vehicleState.Location;
        double kilo = g_stnUp ? g_kiloOffset - loc : g_kiloOffset + loc;   // 上りはキロ程が減る
        if (kilo < 0.0) kilo = 0.0;
        int v = (int)(kilo / 10.0);                              // 10m単位
        int d10km = (v / 1000) % 10;
        panel[g_kiloPanelStart]     = (d10km > 0) ? d10km + 1 : 0;
        panel[g_kiloPanelStart + 1] = (v / 100) % 10 + 1;
        panel[g_kiloPanelStart + 2] = (v / 10) % 10 + 1;
        panel[g_kiloPanelStart + 3] = v % 10 + 1;
    }

    if (g_shouldPlayBell) {
        sound[1] = ATS_SOUND_PLAY; // 1番のインデックスに単発再生命令（1）を送る
        g_shouldPlayBell = false;  // 鳴らしたら即座にフラグを戻す（1フレームだけ鳴らすため）
    }

    // 縦バー形の計器 (INI [Gauge1]〜[Gauge8])
    OutputGauges(vehicleState, panel);
    // パネルの更新間隔・反映遅延 (INI [PanelTiming])。パネル出力をすべて決めたあとに掛ける
    ApplyPanelTiming(panel, currentTimeMs);

    return handles;
}

// その他の必須イベント関数（空実装）この関数がないとビルドエラーになるので、空実装で定義しておく
ATS_API void WINAPI KeyUp(int key) {
    if (key == ATS_KEY_I) {
        g_inching = false;
    }
}
ATS_API void WINAPI DoorOpen()
{
    g_isDoorOpen = true;
    g_psdDoorOpenEvent = true;   // ホームドア: 次の Elapse で開く準備
    OnDoorOpen();
	ATOOnDoorOpen();
}
ATS_API void WINAPI DoorClose() {
	g_isDoorOpen = false;
	g_psdDoorCloseEvent = true;  // ホームドア: 次の Elapse で閉まる準備
	g_psdInstant = false;
	TascOnDoorClose(); // 転動防止ブレーキ(INIの設定でドアが閉まったら緩める)
	ATOOnDoorClose();
}

// 駅ジャンプしたときやシナリオをやり直すときに呼ばれる関数（初期化処理）
ATS_API void WINAPI Initialize(int key)
{
	g_inching = false; //インチングキーをリセット
	ResetGrade(); // 勾配情報をリセット
    g_speed = 0.0f; //速度が0になるため、誤作動対策でこのコードを記述する
	if (g_isAtcPowerOn) {
		g_isAtcPowerOn = false; // ATC電源をOFFにする
        g_isAdvanceNoticeActive = false; // 前方予告作動中フラグをリセット
		g_lastRawSignal = -1; // 信号インデックスを初期化（未受信状態）
	}
	InitTasc(); // TASCの内部状態をリセット
	TascArmHold(); // 止まっていれば、TASC/ATOモードにしたとき転動防止ブレーキを掛ける
	ATOOnInitialize(); // 駅ジャンプ後も、ドアが閉まったら自動発進できるようにする
	g_psdStationHas = g_psdDefault; // 駅ジャンプでは1040を通らないので、INIの DefaultStation に戻す
	g_busGapCount = 0; // 母線引き通し: 駅ジャンプしたら、覚えていた区間を消す
	g_powerOutage = false; // 停電: 駅ジャンプしたら復旧した状態から始める(停電区間の地上子が届けば、また停電になる)
	// ホームドア: 駅ジャンプ直後は、車両のドアが開いていれば動きなしで開いた状態にする
	g_psdInstant = true;
	g_psdWaitOpen = g_psdWaitClose = false;
	g_psdState = PSD_CLOSED;
	g_psdStateUntilMs = 0;
	if (g_isDoorOpen) g_psdDoorOpenEvent = true;
	s_ptLastTimeMs = -1; // パネルの更新間隔・反映遅延: 駅ジャンプのあとは、すぐに今の値を表示する
	if (g_isInNagataStation) {
		g_isInNagataStation = false; // 長田駅エリアフラグをリセット
	}
	g_isLineMismatch = false; // 号線不一致フラグをリセット
	g_isAdvanceNoticeActive = false; // 前方予告作動中フラグをリセット
	// 注意1：ATCFaildフラグはリセットしない（故障状態を保持するため）
	// 注意2: g_isCurrentModeCS はリセットしない（現在のモードを保持するため）
	// 注意3: g_isKeihan はリセットしない（現在のモードを保持するため）
	// 注意4: g_isYardMode はATC電源OFF時に自動的に解除されるため、ここではリセットしない
	// 注意5: g_isOverrideMode はATC電源OFF時に自動的に解除されるため、ここではリセットしない

}
ATS_API void WINAPI HornBlow(int key) {}
