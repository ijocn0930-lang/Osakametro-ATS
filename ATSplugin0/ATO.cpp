#include "ATO.h"
#include "Tasc.h"
#include <Windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// --- ATOの調整値(既定値。ATS.ini の [ATO] で上書きできます) ---
static int   ATO_POWER_NOTCH = 3;                // ATOが出す力行ノッチ(車両の力行段数以下にすること)
static float ATO_CUT_MARGIN_KMH = 5.0f;          // 許容速度の何km/h手前で力行を切るか
static float ATO_RESUME_MARGIN_KMH = 8.0f;       // 許容速度の何km/h手前まで下がったら力行を再開するか
static float ATO_BRAKE_MARGIN_KMH = 3.0f;        // 許容速度の何km/h手前を超えたらブレーキを掛けるか
static float ATO_DECEL_KMHPS = 2.0f;             // 減速パターンの減速度 (km/h/s)
static float ATO_STOP_MARGIN_M = 10.0f;          // 停止信号の何m手前で止まるか
static float ATO_GRADE_LOOKAHEAD_M = 100.0f;     // こう配を先読みする距離 (m)
static float ATO_COAST_LOOKAHEAD_SEC = 5.0f;     // 惰行したときの速度変化を先読みする時間 (秒)
static int   ATO_UPHILL_NOTCH_BONUS_MAX = 2;     // のぼりで力行ノッチを上げる最大段数(10‰ごとに1段)
static int   ATO_EXT_POWER_NOTCHES = 0;          // 力行の拡張ノッチ数(0で使わない。31ならP1〜P31)
static bool  ATO_AUTOSTART_AFTER_JUMP = true;    // true: 駅ジャンプ・シナリオ開始のあと、ドアが閉まったら自動発進する
static const int MAX_DRIVER_MAP = 32;
static int   s_driverMap[MAX_DRIVER_MAP];        // 運転士のP1, P2, … を変換したATOのノッチ(INIの DriverPowerMap)
static int   s_driverMapCount = 0;               // DriverPowerMap に書かれた数
// --- ATOのブレーキ制御の固定値(調整するときは、ここの数字を書き換えます) ---
static const float ATO_BRAKE_DELAY_SEC = 1.0f;     // ブレーキが効き始めるまでに走る時間(秒)。大きいほど、ブレーキが早く始まる
static const int   ATO_BRAKE_RAMP_MS = 400;        // ブレーキを1段ずつ強める間隔(ms)。小さいほど、速く強まる
static const float ATO_BRAKE_OVER_GAIN = 0.5f;     // 速度超過1km/hあたりに、目標減速度へ上乗せする量(km/h/s)
static const float ATO_DECEL_TOL_KMHPS = 0.3f;     // 目標減速度に対する許容幅(km/h/s)
static const float ATO_DECEL_FILTER_SEC = 0.5f;    // 減速度の測定値をなめらかにする時間(秒)
// INIから実数を読む。書かれていない・範囲外のときは既定値を返す
static float IniFloat(const char* key, float def, float minV, float maxV, const char* iniPath) {
    char buf[32] = { 0 };
    GetPrivateProfileStringA("ATO", key, "", buf, sizeof(buf), iniPath);
    if (buf[0] == '\0') return def;
    float v = (float)atof(buf);
    return (v < minV || v > maxV) ? def : v;
}
// INIから整数を読む。書かれていない・範囲外のときは既定値を返す
static int IniInt(const char* key, int def, int minV, int maxV, const char* iniPath) {
    char buf[32] = { 0 };
    GetPrivateProfileStringA("ATO", key, "", buf, sizeof(buf), iniPath);
    if (buf[0] == '\0') return def;
    int v = atoi(buf);
    return (v < minV || v > maxV) ? def : v;
}

// INIから true/false を読む(true, 1 で true)。行の後ろのコメント(; 以降)は取り除く
static bool IniBool(const char* key, bool def, const char* iniPath) {
    char buf[128] = { 0 };
    GetPrivateProfileStringA("ATO", key, "", buf, sizeof(buf), iniPath);
    char* sc = strchr(buf, ';');
    if (sc != NULL) *sc = '\0';
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) buf[--len] = '\0';
    if (buf[0] == '\0') return def;
    return (_stricmp(buf, "true") == 0 || atoi(buf) != 0);
}

// ATS.ini の [ATO] から調整値を読み込む(Load時に1回)
void ATOLoadSettings(const char* iniPath) {
    ATO_AUTOSTART_AFTER_JUMP = IniBool("AutoStartAfterJump", true, iniPath);
    ATO_EXT_POWER_NOTCHES = IniInt("ExtendedPowerNotches", 0, 0, 64, iniPath);
    ATO_POWER_NOTCH = IniInt("PowerNotch", 3, 1, 64, iniPath);
    // DriverPowerMap: カンマ区切り(例: 4, 10, 17, 23, 30)
    {
        char buf[256] = { 0 };
        GetPrivateProfileStringA("ATO", "DriverPowerMap", "", buf, sizeof(buf), iniPath);
        s_driverMapCount = 0;
        char* p = buf;
        while (*p != '\0' && s_driverMapCount < MAX_DRIVER_MAP) {
            while (*p == ' ' || *p == ',' || *p == '\t') p++;
            if (*p == '\0' || *p == ';') break;
            s_driverMap[s_driverMapCount++] = atoi(p);
            while (*p != '\0' && *p != ',') p++;
        }
    }
    ATO_CUT_MARGIN_KMH = IniFloat("CutMargin", 5.0f, 0.0f, 30.0f, iniPath);
    ATO_RESUME_MARGIN_KMH = IniFloat("ResumeMargin", 8.0f, 0.0f, 40.0f, iniPath);
    ATO_BRAKE_MARGIN_KMH = IniFloat("BrakeMargin", 3.0f, 0.0f, 20.0f, iniPath);
    ATO_DECEL_KMHPS = IniFloat("Deceleration", 2.0f, 0.3f, 5.0f, iniPath);
    ATO_STOP_MARGIN_M = IniFloat("StopMargin", 10.0f, 0.0f, 100.0f, iniPath);
    ATO_GRADE_LOOKAHEAD_M = IniFloat("GradeLookahead", 100.0f, 10.0f, 1000.0f, iniPath);
    ATO_COAST_LOOKAHEAD_SEC = IniFloat("CoastLookaheadSec", 5.0f, 0.0f, 30.0f, iniPath);
    ATO_UPHILL_NOTCH_BONUS_MAX = IniInt("UphillNotchBonusMax", 2, 0, 5, iniPath);

    // 再開の速度が切る速度より近いと、力行が細かく入り切りするため補正する
    if (ATO_RESUME_MARGIN_KMH <= ATO_CUT_MARGIN_KMH) {
        ATO_RESUME_MARGIN_KMH = ATO_CUT_MARGIN_KMH + 3.0f;
    }
}
// --- ATO内部状態 ---
static bool s_doorClosed = false;         // ドアが閉まっているか(BVE開始時は開)
static bool s_startRequested = false;    // 発進キーが押された(1回の判定で使い切る)
static bool s_autoStartArmed = false;    // 地上子1003を通過済み(この駅は自動発進)
static bool s_autoStartPending = false;  // ドアが閉まったので、条件が揃い次第、自動発進する
static bool s_hasMoved = false;          // 発進後に動き出したか(停止の誤検知防止)
static bool s_jumpAutoStart = false;     // 駅ジャンプ・シナリオ開始のあと(1003を通れないので、ドアが閉まったら自動発進する)
static bool s_coasting = false;          // 許容速度に近づいたので力行を切っている
static bool  s_nextValid = false;        // 前方予告(次の閉塞の制限速度)を持っているか
static int   s_nextLimitKmh = 0;         // 次の閉塞の制限速度
static float s_nextLocation = 0.0f;      // 次の閉塞の境界位置 (m)
static int   s_atoBrake = 0;             // ATOが出しているブレーキノッチ
static int   s_brakeTimerMs = 0;         // ブレーキを強める間隔のタイマー
static int   s_lastTimeMs = -1;          // 前回のフレーム時刻 (ms)
static float s_lastSpeed = 0.0f;         // 前回のフレームの速度 (km/h)
static float s_decelKmhps = 0.0f;        // 測定した減速度 (km/h/s。減速しているとき正)
// --- 速度制限(地上子1007) ---
struct SpeedLimit { float start; int kmh; };   // start: 制限開始位置(m)
static const int MAX_SPEED_LIMITS = 8;
static SpeedLimit s_limits[MAX_SPEED_LIMITS];
static int s_limitCount = 0;

// 目標速度 targetKmh の地点まで distM メートルあるとき、今この位置で許される速度(km/h)
//   avgGrade: その区間の平均こう配(‰)。のぼりは減速しやすく、くだりは減速しにくい
//   ブレーキが効き始めるまでの時間(ATO_BRAKE_DELAY_SEC)の間に走る距離も見込む
static float PatternSpeedKmh(float targetKmh, float distM, float avgGrade) {
    float vt = targetKmh / 3.6f;                                               // m/s
    float base = ATO_DECEL_KMHPS / 3.6f;                                       // m/s^2
    float a = base + avgGrade * GRADE_ACCEL_MS2_PER_PERMILLE;
    float aMin = base * 0.3f;                                                  // くだりがきつくても、減速度に下限を設ける
    if (a < aMin) a = aMin;
    float T = ATO_BRAKE_DELAY_SEC;
    float v = -a * T + sqrtf(a * a * T * T + vt * vt + 2.0f * a * distM);      // 空走しながら減速して、目標でちょうど目標速度になる速度
    return v * 3.6f;                                                           // km/h
}
// ATOが使うブレーキの段数(TASCと同じ。拡張ノッチを使うときは ExtendedNotches、使わないときは MaxBrake)
static int AtoMaxBrakeStep() {
    return (g_extendedNotches > 0) ? g_extendedNotches : g_maxBrakeNotch;
}
// ATOが使う力行の段数(拡張ノッチを使うときは ExtendedPowerNotches、使わないときは運転士の最大力行)
static int AtoMaxPowerStep() {
    return (ATO_EXT_POWER_NOTCHES > 0) ? ATO_EXT_POWER_NOTCHES : g_maxPowerNotch;
}
int ATOGetExtendedPowerNotches() {
    return ATO_EXT_POWER_NOTCHES;
}
// ATOが出す力行ノッチ(ハンドル出力用)。拡張ノッチのときはそのまま P1〜P(ExtendedPowerNotches)
int ATOGetPower() {
    if (g_ATOStart != 1 || g_ATOPower <= 0) return 0;
    return g_ATOPower;
}
// 運転士の力行ノッチ → ATOのノッチに変換する(拡張ノッチを使わないときは、そのまま)
//   INIの DriverPowerMap があればそれを使う(例: 4, 10, 17, 23, 30)
//   なければ、運転士の最大力行がATOの最大になるよう比例で割り当てる
int ATOConvertDriverPower(int driverPower) {
    if (ATO_EXT_POWER_NOTCHES <= 0 || driverPower <= 0) return driverPower;
    int v;
    if (driverPower <= s_driverMapCount) {
        v = s_driverMap[driverPower - 1];
    }
    else if (s_driverMapCount > 0) {
        v = s_driverMap[s_driverMapCount - 1];   // 変換表より上のノッチ(P5〜など)は、運転士の最大ノッチと同じ扱い
    }
    else if (g_maxPowerNotch > 0) {
        v = (int)((float)driverPower * (float)ATO_EXT_POWER_NOTCHES / (float)g_maxPowerNotch + 0.5f);
    }
    else {
        v = driverPower;
    }
    if (v < 1) v = 1;
    if (v > ATO_EXT_POWER_NOTCHES) v = ATO_EXT_POWER_NOTCHES;
    return v;
}
// 発進待ちに戻す
static void BackToWaiting() {
    g_ATOStart = 0;
    g_ATOPower = 0;
    g_ATOActive = false;
    s_atoBrake = 0;
    s_hasMoved = false;
    s_coasting = false;
}

// ATOを完全に無効化する(ATOモードでない時、ATC電源OFFの時)
void DisableATO() {
    BackToWaiting();
    g_ATOStart = -1;
    s_startRequested = false;
    s_autoStartArmed = false;
    s_autoStartPending = false;
    s_nextValid = false;
	s_limitCount = 0;
}

// ATO初期化(Loadから呼ぶ)
void InitATO() {
    s_doorClosed = false; // 間違えやすいが、BVE開示時は戸開状態です。このためfalseにする
	g_ATO_Failure = false;
    DisableATO();
}

// 駅ジャンプ・シナリオ開始のとき(Initializeから呼ぶ)
//   駅ジャンプでは手前の1003を通らないので、INIで AutoStartAfterJump = true なら自動発進の対象にする
//   ATC電源・ATOモードを入れ直す間も覚えておき、動き出したら忘れる
void ATOOnInitialize() {
    s_doorClosed = false;                         // 駅ジャンプ・開始時は戸開状態
    s_autoStartArmed = false;
    s_autoStartPending = false;
    s_jumpAutoStart = ATO_AUTOSTART_AFTER_JUMP;
}

// ATOが走ってよい基本条件
static bool CanRun() {
    return s_doorClosed
        && g_driverReverser == 1     // 前進
        && g_driverBrake == 0        // 運転士がブレーキを使っていない
        && !g_ATO_Failure
        && !g_TASC_Failure;
}

// 発進してよい状態か(発進キーと自動発進の共通条件)
static bool CanStart(float speedKmh) {
    return CanRun() && !IsTascActive() && speedKmh < 1.0f;
}

// 発進する(発進の瞬間に故障抽選を1回行う)
static void StartRunning() {
    if (ATOCheckRandomFailure()) {
        TriggerAtoFailure();
        s_autoStartPending = false;
        return;
    }
    g_ATOStart = 1;
    s_hasMoved = false;
    s_coasting = false;
    s_autoStartPending = false;
}

void ATOOnDoorOpen() {
    s_doorClosed = false;
    if (g_ATOStart >= 0) BackToWaiting();
    s_startRequested = false;
    s_autoStartPending = false;   // Armed は残す(停車前に通った1003の情報なので)
}

void ATOOnDoorClose() {
    s_doorClosed = true;
    // 1003を通過済みなら、自動発進の待ちにする
    if (s_autoStartArmed && g_TASCATOSet == 2 && g_isAtcPowerOn && g_ATOStart == 0) {
        s_autoStartArmed = false;
        s_autoStartPending = true;
    }
}

// 発進キー: ATOモードで発進待ちのときだけ受け付ける
void ATOOnStartKey() {
    if (g_TASCATOSet == 2 && g_isAtcPowerOn && g_ATOStart == 0) {
        s_startRequested = true;
    }
}

// 自動発進地上子(1003): ATOモードのときだけ記憶する
void ATOOnAutoStartBeacon() {
    if (g_TASCATOSet == 2 && g_isAtcPowerOn) {
        s_autoStartArmed = true;
    }
}

// 前方予告地上子(Type 31): 次の閉塞の制限速度と、境界までの距離を記憶する
void ATOOnAdvanceNotice(int nextLimitKmh, float distanceM, float location) {
    if (g_TASCATOSet == 2 && g_isAtcPowerOn && distanceM > 0.0f) {
        s_nextValid = true;
        s_nextLimitKmh = nextLimitKmh;
        s_nextLocation = location + distanceM;
    }
}

// 速度制限地上子(1007): Optional = 距離(m)×1000 + 制限速度(km/h)
//   例: 600060 → 600m先から60km/h。速度999は制限解除。
void ATOOnSpeedLimitBeacon(int optional, float location) {
    if (g_TASCATOSet != 2 || !g_isAtcPowerOn || optional < 0) return;
    if (s_limitCount >= MAX_SPEED_LIMITS) {          // いっぱいなら古いものから捨てる
        for (int i = 1; i < MAX_SPEED_LIMITS; i++) s_limits[i - 1] = s_limits[i];
        s_limitCount = MAX_SPEED_LIMITS - 1;
    }
    s_limits[s_limitCount].start = location + (float)(optional / 1000);
    s_limits[s_limitCount].kmh = optional % 1000;
    s_limitCount++;
}

// ATOが出すブレーキノッチ(ハンドル出力用。TASCと同じ番号の付け方)
int ATOGetBrake() {
    if (g_ATOStart != 1 || s_atoBrake <= 0) return 0;
    if (g_extendedNotches > 0) return g_maxBrakeNotch + 2 + s_atoBrake;   // 拡張ノッチ
    return s_atoBrake;                                                    // 通常は運転士と同じノッチ
}

// パネル表示用のブレーキ段数(拡張ノッチのときも 1〜ExtendedNotches)
int ATOGetBrakeStep() {
    return (g_ATOStart == 1) ? s_atoBrake : 0;
}

bool ATOIsAutoStartReady() {
	return g_ATOStart >= 0 && (s_autoStartArmed || s_autoStartPending);
}

// 毎フレームの処理
//   atcFree    : ATCが介入していなければ true
//   speedKmh   : 現在速度
//   atcLimitKmh: 現在のATC制限速度
//   location   : 現在位置 (m)
//   timeMs     : 現在時刻 (ms)
void ATOUpdate(bool atcFree, float speedKmh, int atcLimitKmh, float location, int timeMs) {
    // 0. 減速度の測定(ブレーキの強さの調整に使う。毎フレーム更新する)
    int dtMs = 0;
    if (s_lastTimeMs >= 0 && timeMs > s_lastTimeMs) dtMs = timeMs - s_lastTimeMs;
    if (dtMs >= 1000) {                        // 時間が飛んだとき(駅ジャンプなど)
        s_decelKmhps = 0.0f;
        dtMs = 0;
    }
    else if (dtMs > 0) {
        float dt = (float)dtMs / 1000.0f;
        float aInst = (s_lastSpeed - speedKmh) / dt;       // 減速しているとき正 (km/h/s)
        float k = dt / ATO_DECEL_FILTER_SEC;
        if (k > 1.0f) k = 1.0f;
        s_decelKmhps += (aInst - s_decelKmhps) * k;
    }
    s_lastTimeMs = timeMs;
    s_lastSpeed = speedKmh;

    // 駅ジャンプ後の自動発進: 動き出したら(手動で発進したなど)忘れる
    if (s_jumpAutoStart && fabsf(speedKmh) > 1.0f) s_jumpAutoStart = false;

    // 1. ATOモードでない、またはATC電源OFF → 無効化
    if (g_TASCATOSet != 2 || !g_isAtcPowerOn) {
        DisableATO();
        return;
    }
    // 故障中はATOを無効化する(状態表示は「非表示」、故障灯だけ点灯)
    if (g_ATO_Failure) {
        DisableATO();
        return;
    }
    // ATOモードに入った直後は「発進待ち」
    if (g_ATOStart < 0) g_ATOStart = 0;

    // 2. ATC介入中 → 発進待ちに戻す(再発進には発進キーが必要)
    if (!atcFree) {
        BackToWaiting();
        s_startRequested = false;
        s_autoStartPending = false;
        return;
    }

    // 3. 発進待ち
    if (g_ATOStart == 0) {
        g_ATOPower = 0;
        g_ATOActive = false;
        // 駅ジャンプ後: ドアが閉まっていれば自動発進の待ちにする(ドアを閉めてからATOにした場合も含む)
        if (s_jumpAutoStart && s_doorClosed) {
            s_jumpAutoStart = false;
            s_autoStartPending = true;
        }
        if (s_startRequested) {
            s_startRequested = false;   // 受け付けても断っても、要求は1回で使い切る
            if (CanStart(speedKmh)) StartRunning();
        }
        else if (s_autoStartPending && CanStart(speedKmh)) {
            StartRunning();             // 自動発進(条件が揃うまで待ち続ける)
        }
        return;
    }

    // 4. 発進済み(ATO運転中)
    if (!CanRun()) {                       // ドア開・前進でない・運転士のブレーキ・故障
        BackToWaiting();
        return;
    }
    if (speedKmh > 1.0f) s_hasMoved = true;
    if (s_hasMoved && speedKmh < 0.1f) {   // 動いたあとで止まった
        BackToWaiting();
        return;
    }
    // TASCが停車目標を持っている間は力行もATOのブレーキも出さない(ブレーキはTASCが担当)
    if (IsTascActive()) {
        g_ATOPower = 0;
        g_ATOActive = false;
        s_atoBrake = 0;
        return;
    }

    // 今この位置で許される速度 = 制限速度、または前方の制限に向けた減速パターンの低いほう
    float limit = (float)atcLimitKmh;
    int latest = -1;
    for (int i = 0; i < s_limitCount; i++) {
        if (s_limits[i].start <= location &&
            (latest < 0 || s_limits[i].start >= s_limits[latest].start)) latest = i;
    }
    int kept = 0;
    for (int i = 0; i < s_limitCount; i++) {
        bool passed = (s_limits[i].start <= location);
        if (passed && i != latest) continue;
        s_limits[kept++] = s_limits[i];
    }
    s_limitCount = kept;
    // 1007: 現在有効な制限(999は解除)
    for (int i = 0; i < s_limitCount; i++) {
        if (s_limits[i].start <= location && s_limits[i].kmh < 999 &&
            (float)s_limits[i].kmh < limit) limit = (float)s_limits[i].kmh;
    }
    // 1007: 前方の制限に向けた減速パターン
    float baseLimit = limit;
    for (int i = 0; i < s_limitCount; i++) {
        if (s_limits[i].start > location && s_limits[i].kmh < 999 &&
            (float)s_limits[i].kmh < baseLimit) {
            float p = PatternSpeedKmh((float)s_limits[i].kmh, s_limits[i].start - location, GetAverageGrade(location, s_limits[i].start));
            if (p < limit) limit = p;
        }
    }

    // 前方予告(Type 31): 次の閉塞に向けた減速パターン
    if (s_nextValid) {
        float remain = s_nextLocation - location;
        if (remain <= 0.0f) {
            s_nextValid = false;           // 境界を通過した
        }
        else if (s_nextLimitKmh < atcLimitKmh) {
            float d = remain;
            if (s_nextLimitKmh == 0) {     // 停止信号は手前で止まる
                d -= ATO_STOP_MARGIN_M;
                if (d < 0.0f) d = 0.0f;
            }
            float pattern = PatternSpeedKmh((float)s_nextLimitKmh, d, GetAverageGrade(location, s_nextLocation));
            if (pattern < limit) limit = pattern;
        }
    }

    // ブレーキ: パターン(余裕込み)を超えたら掛け、1km/h以上低くなったら緩める
    int brakeMax = AtoMaxBrakeStep();                                  // TASCと同じ段数
    int rampMs = (int)((float)ATO_BRAKE_RAMP_MS * (float)g_maxBrakeNotch / (float)brakeMax);   // 段数が細かいときは間隔を短くする
    if (rampMs < 50) rampMs = 50;
    float over = speedKmh - (limit - ATO_BRAKE_MARGIN_KMH);
    if (over > 0.0f) {
        // 目標減速度(こう配を含む)。パターンを超えている分だけ上乗せする
        float gradeNow = GetAverageGrade(location, location + ATO_GRADE_LOOKAHEAD_M);
        float aTarget = ATO_DECEL_KMHPS + gradeNow * GRADE_ACCEL_MS2_PER_PERMILLE * 3.6f;
        if (aTarget < ATO_DECEL_KMHPS * 0.3f) aTarget = ATO_DECEL_KMHPS * 0.3f;
        aTarget += ATO_BRAKE_OVER_GAIN * over;

        if (s_atoBrake == 0) {                 // ブレーキ開始: まず1段目
            s_atoBrake = 1;
            s_brakeTimerMs = 0;
        }
        else {                                 // 目標減速度に届くまで、一定の間隔で1段ずつ強める(直線的に強くなる)
            s_brakeTimerMs += dtMs;
            if (s_brakeTimerMs >= rampMs) {
                s_brakeTimerMs = 0;
                if (s_decelKmhps < aTarget - ATO_DECEL_TOL_KMHPS && s_atoBrake < brakeMax) {
                    s_atoBrake++;
                }
                else if (s_decelKmhps > aTarget + ATO_DECEL_TOL_KMHPS * 3.0f && s_atoBrake > 1) {
                    s_atoBrake--;              // 強すぎるときだけ弱める
                }
            }
        }
    }
    else if (over < -1.0f) {
        s_atoBrake = 0;
        s_brakeTimerMs = 0;
    }
    if (s_atoBrake > 0) {                      // ブレーキ中は力行しない
        g_ATOPower = 0;
        g_ATOActive = false;
        s_coasting = true;
        return;
    }

    // 力行を続けてよい速度: 前方予告で次の閉塞が低いときは、その速度を超えて加速しない
    float powerLimit = limit;
    if (s_nextValid && s_nextLimitKmh < atcLimitKmh && (float)s_nextLimitKmh < powerLimit) {
        powerLimit = (float)s_nextLimitKmh;
    }

    // 力行: 許容速度に近づいたら切る(切る/再開の速度に差をつけて、細かい入り切りを防ぐ)
    // こう配による速度変化の先読み: 惰行したときの加速度(km/h/s)。くだりはプラス、のぼりはマイナス
    float grade = GetAverageGrade(location, location + ATO_GRADE_LOOKAHEAD_M);
    float aGrade = -grade * GRADE_ACCEL_MS2_PER_PERMILLE * 3.6f;
    float shift = fabsf(aGrade) * ATO_COAST_LOOKAHEAD_SEC;
    float cutSpeed, resumeSpeed;
    if (aGrade > 0.0f) {        // くだり: 惰行でも加速するので、早めに切り、再開も遅くする
        cutSpeed = powerLimit - ATO_CUT_MARGIN_KMH - shift;
        resumeSpeed = powerLimit - ATO_RESUME_MARGIN_KMH - shift;
    }
    else {                      // のぼり(平坦を含む): 切るのを遅らせて、入り切りの回数を減らす
        cutSpeed = powerLimit - ATO_CUT_MARGIN_KMH + shift;
        if (cutSpeed > powerLimit - 1.0f) cutSpeed = powerLimit - 1.0f;
        resumeSpeed = powerLimit - ATO_RESUME_MARGIN_KMH;
    }
    if (s_coasting) {
        if (speedKmh < resumeSpeed) s_coasting = false;
    }
    else {
        if (speedKmh >= cutSpeed) s_coasting = true;
    }
    if (s_coasting) {
        g_ATOPower = 0;
        g_ATOActive = false;
    }
    else {
        // のぼりは力行ノッチを上げる(10‰ごとに運転士の1段ぶん。拡張ノッチのときは段数を換算する)
        int notch = ATO_POWER_NOTCH;
        int maxStep = AtoMaxPowerStep();
        if (grade > 0.0f) {
            int bonus = (int)(grade / 10.0f);
            if (bonus > ATO_UPHILL_NOTCH_BONUS_MAX) bonus = ATO_UPHILL_NOTCH_BONUS_MAX;
            int driverMax = (s_driverMapCount > 0) ? s_driverMapCount : g_maxPowerNotch;   // 運転士の最大力行
            float scale = (ATO_EXT_POWER_NOTCHES > 0 && driverMax > 0)
                ? (float)ATO_EXT_POWER_NOTCHES / (float)driverMax : 1.0f;
            notch += (int)((float)bonus * scale + 0.5f);
        }
        if (maxStep > 0 && notch > maxStep) notch = maxStep;
        g_ATOPower = notch;
        g_ATOActive = true;
    }
}
// ATO故障の確率判定
bool ATOCheckRandomFailure() {
    if (!g_ATO_FailureActive || g_ATO_FailureRate <= 0.0f) return false;
    if (g_ATO_FailureRate >= 1.0f) return true; // 確率1.0なら確実に故障
    float randVal = (float)rand() / (float)RAND_MAX;
    return (randVal < g_ATO_FailureRate);
}

void TriggerAtoFailure() {
    g_ATO_Failure = true;
    g_ATOActive = false;
    g_ATOPower = 0;
    g_ATOStart = -1;
}