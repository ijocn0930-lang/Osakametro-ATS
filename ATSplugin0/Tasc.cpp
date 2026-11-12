#include "Tasc.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>



// --- 設定値・制御変数 ---
static std::vector<float> s_pressureRates;          // 圧力比率テーブル

static int s_currentTascBrake = 0;                  // 現在のTASC内部ノッチ (0 〜 s_extendedNotches)
static int s_lastTimeMs = -1; // 前回のフレーム時刻 (ms)
static int s_notchTimerMs = 0; // ノッチ変化タイマー (ms)
static float s_targetLocation = -1.0f;   // 目標停止位置 (m)
static float s_stopTolerance = 0.5f;     // 許容範囲 (m) 既定値: ±50cm (0.5m)
static float s_pendingRelativeDist = -1.0f; // 保留中の相対距離 (m) -1.0f で保留なしを示す
static float s_lampTarget = -1.0f;       // 定点停止灯を点けたときの停車位置 (m)。-1.0f でなし
static float s_lampTolerance = 0.5f;     // 定点停止灯を点けたときの許容範囲 (m)

// --- TASCの調整値(既定値。ATS.ini の [TASC] で上書きできます) ---
static float TASC_DELAY_TIME_SEC = 1.0f;         // 空走時間 (秒)
static int   NOTCH_STEP_INTERVAL_MS = 100;       // ノッチを1段ずつ変える間隔 (ms)
static float TASC_DEADBAND_KMH = 1.0f;           // パターンより遅いとき、現在のノッチを維持する幅 (km/h)
static float TASC_FINAL_DIST_M = 5.0f;           // 最終接近区間の距離 (m)
static float TASC_FINAL_MIN_RATIO = 0.25f;       // 最終接近区間で下げない最小ノッチの割合
static float TASC_RELEASE_RATIO = 0.2f;          // 停車直前の微低速域で弱めるノッチの割合
static float INCHING_MAX_DIST_M = 10.0f;         // インチングを使えるずれの上限 (m)
static float INCHING_SPEED_KMH = 2.0f;           // インチングで動かす速度の上限 (km/h)
static int   INCHING_POWER_NOTCH = 1;            // インチングの力行ノッチ
static int   INCHING_BRAKE_NOTCH = 2;            // インチングで止めるときのブレーキノッチ
static float INCHING_LEAD_SEC = 0.8f;            // 止まるまでの余裕時間 (秒)
static float TASC_HOLD_RATIO = 1.0f;             // 停車中の転動防止ブレーキの強さ(最大ノッチに対する割合)
static bool  TASC_HOLD_RELEASE_DOOR_OPEN = false;  // true: ドアが開いたら転動防止ブレーキを緩める
static bool  TASC_HOLD_RELEASE_DOOR_CLOSE = false; // true: ドアが閉まったら転動防止ブレーキを緩める
static float TASC_HOLD_RELEASE_DELAY_SEC = 0.0f;   // 運転士がブレーキを操作してから緩めるまでの時間(秒)

// --- 停車中の転動防止(停車保持ブレーキ) ---
//   TASCで停車したとき、またはTASC/ATOモードでドアが開いたときに掛ける。
//   解除: 運転士のブレーキが保持ブレーキ以上になった、ATOが発進した、手動モード・ATC電源OFF、速度が出た(5km/h超)
extern int g_ATOStart;                           // ATO発進状態(1で発進済み)
static bool s_holdActive = false;
static bool s_holdArmed = false;     // シナリオ開始・駅ジャンプ・モード切替のあと、止まっていれば保持を始める
static bool s_holdPrevCovers = true; // 前のフレームで運転士のブレーキが保持ブレーキ以上だったか
static bool s_holdReleasePending = false; // 運転士が操作したので、遅延時間のあとに緩める
static int  s_holdReleaseAtMs = 0;        // 緩める時刻 (ms)
// 保持を始める(運転士がすでに強いブレーキを使っていても、いったん弱めてから強めるまでは解除しない)
static void StartHold() {
    s_holdActive = true;
    s_holdPrevCovers = true;
    s_holdReleasePending = false;
}
// ドアが閉まったとき(INIで HoldReleaseOnDoorClose = true なら緩める)
void TascOnDoorClose() {
    if (TASC_HOLD_RELEASE_DOOR_CLOSE) {
        s_holdActive = false;
        s_holdArmed = false;
        s_holdReleasePending = false;
    }
}
// シナリオ開始・駅ジャンプ・モード切替のときに呼ぶ
void TascArmHold() {
    s_holdArmed = true;
}







// INIから実数を読む。書かれていない・範囲外のときは既定値を返す
static float TascIniFloat(const char* key, float def, float minV, float maxV, const char* iniPath) {
    char buf[32] = { 0 };
    GetPrivateProfileStringA("TASC", key, "", buf, sizeof(buf), iniPath);
    if (buf[0] == '\0') return def;
    float v = (float)atof(buf);
    return (v < minV || v > maxV) ? def : v;
}
// INIから整数を読む。書かれていない・範囲外のときは既定値を返す
static int TascIniInt(const char* key, int def, int minV, int maxV, const char* iniPath) {
    char buf[32] = { 0 };
    GetPrivateProfileStringA("TASC", key, "", buf, sizeof(buf), iniPath);
    if (buf[0] == '\0') return def;
    int v = atoi(buf);
    return (v < minV || v > maxV) ? def : v;
}

// INIから true/false を読む(true, 1 で true)。書かれていないときは既定値を返す
static bool TascIniBool(const char* key, bool def, const char* iniPath) {
    char buf[128] = { 0 };
    GetPrivateProfileStringA("TASC", key, "", buf, sizeof(buf), iniPath);
    // 行の後ろのコメント(; 以降)と空白を取り除く(Windowsはコメントを取り除かないため)
    char* sc = strchr(buf, ';');
    if (sc != NULL) *sc = '\0';
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) buf[--len] = '\0';
    if (buf[0] == '\0') return def;
    return (_stricmp(buf, "true") == 0 || atoi(buf) != 0);
}

// ATS.ini の [TASC] から調整値を読み込む(Load時に1回)
void TascLoadSettings(const char* iniPath) {
    TASC_DELAY_TIME_SEC = TascIniFloat("DelayTime", 1.0f, 0.0f, 5.0f, iniPath);
    NOTCH_STEP_INTERVAL_MS = TascIniInt("NotchStepInterval", 100, 10, 1000, iniPath);
    TASC_DEADBAND_KMH = TascIniFloat("DeadBand", 1.0f, 0.0f, 5.0f, iniPath);
    TASC_FINAL_DIST_M = TascIniFloat("FinalApproachDist", 5.0f, 1.0f, 30.0f, iniPath);
    TASC_FINAL_MIN_RATIO = TascIniFloat("FinalMinNotchRatio", 0.25f, 0.0f, 1.0f, iniPath);
    TASC_RELEASE_RATIO = TascIniFloat("ReleaseNotchRatio", 0.2f, 0.0f, 1.0f, iniPath);
    INCHING_MAX_DIST_M = TascIniFloat("InchingMaxDist", 10.0f, 0.5f, 100.0f, iniPath);
    INCHING_SPEED_KMH = TascIniFloat("InchingSpeed", 2.0f, 0.5f, 10.0f, iniPath);
    INCHING_POWER_NOTCH = TascIniInt("InchingPowerNotch", 1, 1, 10, iniPath);
    INCHING_BRAKE_NOTCH = TascIniInt("InchingBrakeNotch", 2, 1, 10, iniPath);
    INCHING_LEAD_SEC = TascIniFloat("InchingLeadTime", 0.8f, 0.0f, 5.0f, iniPath);
    TASC_HOLD_RATIO = TascIniFloat("HoldBrakeRatio", 1.0f, 0.1f, 1.0f, iniPath);
    TASC_HOLD_RELEASE_DOOR_OPEN = TascIniBool("HoldReleaseOnDoorOpen", false, iniPath);
    TASC_HOLD_RELEASE_DOOR_CLOSE = TascIniBool("HoldReleaseOnDoorClose", false, iniPath);
    TASC_HOLD_RELEASE_DELAY_SEC = TascIniFloat("HoldReleaseDelay", 0.0f, 0.0f, 30.0f, iniPath);
}
// --- ブレーキの強め方の固定値(ATOと同じ考え方。調整するときは、ここの数字を書き換えます) ---
static const int   TASC_RAMP_MS = 600;             // ブレーキを1段ずつ強める間隔(ms)。小さいほど、速く強まる
static const float TASC_RAMP_MARGIN_KMH = 0.1f;    // パターンの何km/h手前からブレーキを始めるか
static const float TASC_RAMP_OVER_GAIN = 0.4f;     // 速度超過1km/hあたりに、目標減速度へ上乗せする量(km/h/s)
static const float TASC_DECEL_TOL_KMHPS = 0.5f;    // 目標減速度に対する許容幅(km/h/s)
static const float TASC_DECEL_FILTER_SEC = 0.5f;   // 減速度の測定値をなめらかにする時間(秒)
static const int   TASC_HOLD_MS = 1000;   // ノッチ変更後に効果を待つ時間
static const int   TASC_MAX_STEP = 6;      // 1回で動かす最大ノッチ数
static const float TASC_ERR_TOL = 0.2f;   // 許容する減速度誤差(km/h/s)
static const float TASC_LOW_SPEED_KMH = 7.0f;   // これより遅いときは減速度の測定値を使わず、パターンとの差だけで動かす
static float s_tascKPerNotch = 0.13f;  // 1ノッチあたりの減速度(走行中に学習)
static float s_tascDecelKmhps = 0.0f;              // 測定した減速度 (km/h/s。減速しているとき正)
static float s_tascDecelLastSpeed = 0.0f;          // 前回のフレームの速度 (km/h)
static int   s_tascDecelLastTime = -1;             // 前回のフレーム時刻 (ms)
static float s_tascDecelTrend = 0.0f;              // 減速度の変化の傾き (km/h/s^2)
static const float TASC_DECEL_LEAD_SEC = 1.0f;     // ブレーキが効くまでの遅れを見込む先読み時間(秒)
static const float TASC_RAMP_UNDER_GAIN = 1.0f;   // パターンより遅いとき、速度差1km/hあたりに目標減速度を下げる量(km/h/s)







// 実際の減速度を測る(毎フレーム呼ぶ)
static void UpdateMeasuredDecel(float speedKmh, int timeMs) {
    int dtMs = 0;
    if (s_tascDecelLastTime >= 0 && timeMs > s_tascDecelLastTime) dtMs = timeMs - s_tascDecelLastTime;
    if (dtMs >= 1000) {                        // 時間が飛んだとき(駅ジャンプなど)
        s_tascDecelKmhps = 0.0f;
        s_tascDecelTrend = 0.0f;
    }
    else if (dtMs > 0) {
        float dt = (float)dtMs / 1000.0f;
        float aInst = (s_tascDecelLastSpeed - speedKmh) / dt;   // 減速しているとき正
        float k = dt / TASC_DECEL_FILTER_SEC;
        if (k > 1.0f) k = 1.0f;
        float prev = s_tascDecelKmhps;
        s_tascDecelKmhps += (aInst - prev) * k;
        float rawTrend = (s_tascDecelKmhps - prev) / dt;        // 減速度の変化の傾き
        s_tascDecelTrend += (rawTrend - s_tascDecelTrend) * k;
    }
    s_tascDecelLastTime = timeMs;
    s_tascDecelLastSpeed = speedKmh;
}
// --- インチング(目標位置の手前・奥で止まったとき、キーを押している間だけ少し動いて位置を合わせる) ---
static bool s_inchingActive = false;    // 動かしている
static bool s_inchingBraking = false;   // 止めている(ブレーキ中)
static int  s_inchingDir = 0;           // +1:前進、-1:後進
static int  s_inchingPower = 0;
static int  s_inchingBrake = 0;
static bool s_inchingPrevKey = false;   // 前フレームのキーの状態(押した瞬間の検出用)

int TascInchingPower() {
    return s_inchingPower;
}
int TascInchingBrake() {
    return s_inchingBrake;
}
int TascInchingReverser() {
    return s_inchingDir;
}
bool TascIsInching() {
    return s_inchingActive || s_inchingBraking;
}

bool TascInchingUpdate(bool keyHeld, bool atcFree, float location, float speedKmh,
    int driverBrake, int driverPower, int driverReverser) {
    bool pressed = keyHeld && !s_inchingPrevKey;   // キーを押した瞬間
    s_inchingPrevKey = keyHeld;

    float speed = fabsf(speedKmh);
    bool tascOk = (g_TASCATOSet != 0) && g_isAtcPowerOn && !g_TASC_Failure
        && s_isTascActive && s_targetLocation > 0.0f;
    if (!atcFree || !tascOk) {          // ATCが介入、またはTASCが使えない → 終了(ブレーキはATC/TASCに任せる)
        s_inchingActive = false;
        s_inchingBraking = false;
        s_inchingDir = 0;
        s_inchingPower = 0;
        s_inchingBrake = 0;
        return false;
    }

    float rem = s_targetLocation - location;   // 正:手前、負:行き過ぎ
    float dist = fabsf(rem);

    // 開始: 停止中にキーを押し、許容範囲の外で、近くにあるとき
    if (!s_inchingActive && !s_inchingBraking) {
        if (pressed && driverBrake == 0 && driverPower == 0 && driverReverser != 0 &&
            speed < 0.5f && dist > s_stopTolerance && dist <= INCHING_MAX_DIST_M) {
            s_inchingActive = true;
            s_inchingDir = (rem > 0.0f) ? 1 : -1;
        }
        else {
            return false;
        }
    }

    float speedMS = speed / 3.6f;
    float stopBand = s_stopTolerance * 0.5f + speedMS * INCHING_LEAD_SEC;   // この距離まで来たら止めにかかる
    bool reached = (dist <= stopBand) || (rem * (float)s_inchingDir <= 0.0f);

    // 終了: キーを離した・運転士が操作した・位置に着いた → ブレーキで止める
    if (s_inchingActive && (!keyHeld || driverBrake != 0 || driverPower != 0 || reached)) {
        s_inchingActive = false;
        s_inchingBraking = true;
    }
    if (s_inchingBraking) {
        if (speed < 0.1f) {             // 止まった → 以降はTASCが位置を確定する
            s_inchingBraking = false;
            s_inchingDir = 0;
            s_inchingPower = 0;
            s_inchingBrake = 0;
            return false;
        }
        s_inchingPower = 0;
        s_inchingBrake = (INCHING_BRAKE_NOTCH < g_maxBrakeNotch) ? INCHING_BRAKE_NOTCH : g_maxBrakeNotch;
        return true;
    }

    // 動かしている間
    int pw = INCHING_POWER_NOTCH;
    if (g_maxPowerNotch > 0 && pw > g_maxPowerNotch) pw = g_maxPowerNotch;
    s_inchingBrake = 0;
    s_inchingPower = (speed < INCHING_SPEED_KMH) ? pw : 0;
    return true;
}





// --- こう配(地上子1008) ---
struct GradePoint { float start; float permille; };   // start: こう配が変わる位置(m)
static const int MAX_GRADE_POINTS = 16;
static GradePoint s_grades[MAX_GRADE_POINTS];         // 位置の小さい順に並べて保持する
static int s_gradeCount = 0;

void ResetGrade() {
    s_gradeCount = 0;
}

// 位置 pos でのこう配(‰)。情報がなければ 0(平坦)
static float GradeAt(float pos) {
    float g = 0.0f;
    for (int i = 0; i < s_gradeCount; i++) {
        if (s_grades[i].start <= pos) g = s_grades[i].permille;
        else break;
    }
    return g;
}

// 地上子1008: Optional = ±(距離(m)×1000 + こう配(‰))  負数はくだり
//   例: 300015 → 300m先から +15‰(のぼり)、-300020 → 300m先から -20‰(くだり)
void ProcessGradeBeacon(int optional, float location) {
    int mag = (optional < 0) ? -optional : optional;
    float start = location + (float)(mag / 1000);
    float permille = (float)(mag % 1000);
    if (optional < 0) permille = -permille;

    if (s_gradeCount >= MAX_GRADE_POINTS) {            // いっぱいなら古いものから捨てる
        for (int i = 1; i < MAX_GRADE_POINTS; i++) s_grades[i - 1] = s_grades[i];
        s_gradeCount = MAX_GRADE_POINTS - 1;
    }
    int pos = s_gradeCount;                            // 位置順に挿入する
    while (pos > 0 && s_grades[pos - 1].start > start) {
        s_grades[pos] = s_grades[pos - 1];
        pos--;
    }
    s_grades[pos].start = start;
    s_grades[pos].permille = permille;
    s_gradeCount++;
}

// from から to までの平均こう配(‰)
float GetAverageGrade(float from, float to) {
    if (s_gradeCount == 0) return 0.0f;
    if (to <= from) return GradeAt(from);
    float sum = 0.0f;
    float pos = from;
    float g = GradeAt(from);
    for (int i = 0; i < s_gradeCount; i++) {
        if (s_grades[i].start <= pos) continue;
        if (s_grades[i].start >= to) break;
        sum += g * (s_grades[i].start - pos);
        pos = s_grades[i].start;
        g = s_grades[i].permille;
    }
    sum += g * (to - pos);
    return sum / (to - from);
}

void InitTasc() { // TASC内部状態の初期化　★g_TASC_Failureはリセットしない（故障状態を保持するため）
    s_isTascActive = false;
    s_targetLocation = -1.0f;
    s_pendingRelativeDist = -1.0f;
    s_stopTolerance = g_defaultClearance;
    s_currentTascBrake = 0;
    s_lastTimeMs = -1;
    s_notchTimerMs = 0;
    g_PositionEnable = false;
    s_holdActive = false;
}

bool IsTascActive() { // TASC作動中フラグの取得関数
    return s_isTascActive;
}


// 内部計算用の最大ノッチ段数を取得
static int GetEffectiveMaxNotch() {
    return (g_extendedNotches > 0) ? g_extendedNotches : g_maxBrakeNotch;
}
// 停車保持ブレーキの段数(TASCの段数で 1〜最大)
static int HoldStep() {
    int n = (int)((float)GetEffectiveMaxNotch() * TASC_HOLD_RATIO + 0.5f);
    if (n < 1) n = 1;
    return n;
}
// 運転士のブレーキが保持ブレーキ以上か(最大ノッチに対する割合で比べる。非常は常に以上)
static bool DriverBrakeCoversHold(int driverBrake) {
    if (driverBrake <= 0 || g_maxBrakeNotch <= 0) return false;
    if (driverBrake > g_maxBrakeNotch) return true;
    float driverRatio = (float)driverBrake / (float)g_maxBrakeNotch;
    float holdRatio = (float)HoldStep() / (float)GetEffectiveMaxNotch();
    return driverRatio >= holdRatio - 0.001f;
}
// 地上子データの処理
void ProcessTascBeacon(int type, int optional) {
    if (!g_isAtcPowerOn) return;

    if (type == 1030) {
        float relativeDistanceM = (float)optional / 100.0f;
        if (relativeDistanceM > 0.0f) {
            s_pendingRelativeDist = relativeDistanceM;
        }
    }
    else if (type == 1031) {
        if (optional > 0) {
            s_stopTolerance = (float)optional / 100.0f;
        }
        else {
            s_stopTolerance = g_defaultClearance;
        }
    }
}
// 始発駅・駅ジャンプのあとの定点停止灯(地上子1041)
static bool  s_startLampPending = false;  // 1041を受けた(止まったまま、まだ動いていない)
static float s_startLampLoc = -1.0f;      // 1041を受けたときの位置 (m)。-1.0f は次の Elapse で決める
static float s_startLampTol = 0.5f;       // 許容範囲 (m)
void TascOnStationLampBeacon(int optional) {
    s_startLampPending = true;
    s_startLampLoc = -1.0f;
    s_startLampTol = (optional > 0) ? (float)optional / 100.0f : g_defaultClearance;
}
// 毎フレーム呼ぶ: 動いたら(許容範囲を超えたら)取り消す
void TascTrackStartLamp(float location, float speed) {
    if (!s_startLampPending) return;
    if (s_startLampLoc < 0.0f) {
        if (fabsf(speed) < 0.1f) s_startLampLoc = location;  // 止まっていれば、この位置を停車位置にする
        else s_startLampPending = false;                     // 走行中に受けたときは使わない
        return;
    }
    if (fabsf(location - s_startLampLoc) > s_startLampTol) s_startLampPending = false;
}
// ドアが開いたらTASCをリセットする
// g_TASCFailure変数は故障状態を保持するため、リセットしない
void OnDoorOpen() {
    s_isTascActive = false;
    s_targetLocation = -1.0f;
    s_pendingRelativeDist = -1.0f;
    s_currentTascBrake = 0;
    s_lastTimeMs = -1;
    s_notchTimerMs = 0;
    // 定点停止灯はドアが開いても消さない(停車位置から許容範囲を超えて動いたら消す)
	s_stopTolerance = g_defaultClearance;
    // TASC/ATOモードでは、ドアが開いている間も停車保持ブレーキを掛け続ける(転動防止)
    //   INIで HoldReleaseOnDoorOpen = true のときは、ドアが開いたら緩める
    if (TASC_HOLD_RELEASE_DOOR_OPEN) {
        s_holdActive = false;
        s_holdArmed = false;
        s_holdReleasePending = false;
    }
    else if (g_TASCATOSet != 0 && g_isAtcPowerOn) {
        StartHold();
    }
}
// TASC故障を発生させる内部関数
void TriggerTascFailure() {
    g_TASC_Failure = true;
    s_isTascActive = false;
    s_currentTascBrake = 0;
    g_PositionEnable = false;
}

// 確率判定ロジック (0.0〜1.0 の設定値に基づく)
bool CheckRandomFailure() {
    if (!g_TASC_FailureActive || g_TASC_FailureRate <= 0.0f) return false;
    if (g_TASC_FailureRate >= 1.0f) return true; // 確率1.0なら確実に故障

    // 0.0 〜 1.0 の乱数を生成して比較
    float randVal = (float)rand() / (float)RAND_MAX;
    return (randVal < g_TASC_FailureRate);
}
// TASCスイッチの切り替え処理
void OnTascSwitchToggled(int newState) {
    g_TASCATOSet = newState; // 0: 切, 1: 入

    // 既に故障している状態で「入」に戻された場合
    if (g_TASC_Failure && g_TASCATOSet != 0) {
        // 次の Elapse から自動的に非常ブレーキが割り込みます
        g_PositionEnable = false;
    }
}
// TASCブレーキ演算処理
int CalculateTascBrake(float currentLocation, float currentSpeed, int driverBrake, int currentTimeMs) {
    UpdateMeasuredDecel(fabsf(currentSpeed), currentTimeMs);   // ★追加: 実際の減速度を測る
    // 定点停止灯: 点灯したときの停車位置から、許容範囲を超えて動いたら消灯する
    if (g_PositionEnable && s_lampTarget > 0.0f &&
        fabsf(currentLocation - s_lampTarget) > s_lampTolerance) {
        g_PositionEnable = false;
        s_lampTarget = -1.0f;
    }
    // 始発駅・駅ジャンプのあと(地上子1041): ATC電源が入っていて止まっていれば点灯する
    if (s_startLampPending && s_startLampLoc >= 0.0f && !g_PositionEnable &&
        !s_isTascActive && fabsf(currentSpeed) < 0.1f) {
        g_PositionEnable = true;
        s_lampTarget = s_startLampLoc;
        s_lampTolerance = s_startLampTol;
    }
	// 故障判定
	if (g_TASC_Failure) {
		s_currentTascBrake = 0; // 故障中はTASC制御を無効化
		s_isTascActive = false; // 故障中はTASC制御を無効化
		g_PositionEnable = false; // 故障中は定点停止灯を消灯
        if (g_TASCATOSet != 0) {
            int ebNotch = g_maxBrakeNotch + 1; // 非常ブレーキノッチ
            return (driverBrake > ebNotch) ? driverBrake : ebNotch; // 故障中は非常ブレーキを掛ける
        }
        else {
			return driverBrake; // 故障中はTASC制御を無効化
        }
	}
    // 停車保持ブレーキ(転動防止): 解除の条件を確かめ、TASCが停車目標を持っていない間はここで保持する
    if (s_holdArmed) {                       // シナリオ開始・駅ジャンプ・モード切替のあと
        if (fabsf(currentSpeed) > 0.5f) {
            s_holdArmed = false;                 // 動いていれば保持しない
        }
        else if (g_TASCATOSet != 0 && g_isAtcPowerOn) {
            s_holdArmed = false;
            if (!s_holdActive) StartHold();      // 止まっていれば保持を始める
        }
    }
    if (s_holdActive) {
        // 運転士のブレーキが保持ブレーキより弱い状態から、保持ブレーキ以上に強めたら解除(引き継ぎ)
        bool covers = DriverBrakeCoversHold(driverBrake);
        if (covers && !s_holdPrevCovers && !s_holdReleasePending) {
            s_holdReleasePending = true;       // 遅延時間(HoldReleaseDelay)のあとに緩める
            s_holdReleaseAtMs = currentTimeMs + (int)(TASC_HOLD_RELEASE_DELAY_SEC * 1000.0f);
        }
        s_holdPrevCovers = covers;
        bool driverTakeover = s_holdReleasePending && currentTimeMs >= s_holdReleaseAtMs;
        if (g_TASCATOSet == 0 || !g_isAtcPowerOn || g_ATOStart == 1 ||
            fabsf(currentSpeed) > 5.0f || driverTakeover) {
            s_holdReleasePending = false;
            s_holdActive = false;
            if (!s_isTascActive) s_currentTascBrake = 0;
        }
        else if (!s_isTascActive && s_pendingRelativeDist <= 0.0f) {
            s_currentTascBrake = HoldStep();
            int holdOut = (g_extendedNotches > 0) ? g_maxBrakeNotch + 2 + s_currentTascBrake : s_currentTascBrake;
            return (driverBrake > holdOut) ? driverBrake : holdOut;
        }
    }
	if (currentLocation <= 0.0f) return driverBrake; // 現在位置が不明な場合はTASC制御を無効化

    int deltaTimeMs = 0;
    if (s_lastTimeMs >= 0) {
        deltaTimeMs = currentTimeMs - s_lastTimeMs;
        if (deltaTimeMs < 0) deltaTimeMs = 0;
    }
    s_lastTimeMs = currentTimeMs;

    // 地上子データの同期処理
    if (s_pendingRelativeDist > 0.0f) {
        bool wasActive = s_isTascActive;                    // すでに作動中か(2つ目以降の地上子)
        s_targetLocation = currentLocation + s_pendingRelativeDist;
        s_isTascActive = true;
        s_pendingRelativeDist = -1.0f;

        if (!wasActive) {                                   // 最初の地上子のときだけ初期化と故障抽選を行う
            s_currentTascBrake = 0;
            g_PositionEnable = false;

            if (g_TASCATOSet != 0 && CheckRandomFailure()) {
                TriggerTascFailure();
                int ebNotch = g_maxBrakeNotch + 1;
                return (driverBrake > ebNotch) ? driverBrake : ebNotch;
            }
        }
    }

    // 非作動時の処理
    if (g_TASCATOSet == 0 || !g_isAtcPowerOn || !s_isTascActive || s_targetLocation <= 0.0f) {
        s_currentTascBrake = 0;
        return driverBrake;
    }


    int effectiveMaxNotch = GetEffectiveMaxNotch();
    float remainingDistance = s_targetLocation - currentLocation;
    float absSpeed = fabsf(currentSpeed);

    if (remainingDistance < -100.0f) return driverBrake;
    // ----------------------------------------------------
        // 条件1: 許容範囲内（例: ±0.5m以内）に収まった場合
        // ----------------------------------------------------
    if (fabsf(remainingDistance) <= s_stopTolerance) {
        // 微低速または全停止状態であれば速やかに最大ノッチへ上げて位置確定
        if (absSpeed < 0.1f) {
            g_PositionEnable = true;
            s_lampTarget = s_targetLocation;      // 定点停止灯: この位置から許容範囲を超えて動いたら消灯する
            s_lampTolerance = s_stopTolerance;
            if (!s_holdActive) StartHold();   // 停車した: ドアが開いたあとも転動防止ブレーキを続ける
            s_currentTascBrake = effectiveMaxNotch; // 即座に最大ノッチ化（タイマースキップ）
            s_notchTimerMs = 0;

            if (g_extendedNotches > 0) {
                int offset = g_maxBrakeNotch + 2;
                return offset + s_currentTascBrake;
            }
            return g_maxBrakeNotch;
        }
        // ② 停車直前の微低速域 (0.5km/h未満) -> 衝動緩和のためブレーキを弱める (B1~B2相当)
        else if (absSpeed < 0.5f) {
            int releaseNotch = (int)(effectiveMaxNotch * TASC_RELEASE_RATIO); // 全体の約20%（例: 14段中B2-B3、7段中B1）
            if (releaseNotch < 1) releaseNotch = 1;

            s_currentTascBrake = releaseNotch;
            s_notchTimerMs = 0;

            if (g_extendedNotches > 0) {
                int offset = g_maxBrakeNotch + 2;
                return (driverBrake > (offset + s_currentTascBrake)) ? driverBrake : (offset + s_currentTascBrake);
            }
            return (driverBrake > s_currentTascBrake) ? driverBrake : s_currentTascBrake;
        }
    }
    else {
        g_PositionEnable = false;
    }
        // ----------------------------------------------------
        // 条件2: オーバーラン確定時（許容範囲を超えて手前/奥に脱走）
        // ----------------------------------------------------
        if (remainingDistance < -s_stopTolerance) {
            s_currentTascBrake = effectiveMaxNotch;
            if (g_extendedNotches > 0) {
                int offset = g_maxBrakeNotch + 2;
                return offset + s_currentTascBrake;
            }
            return g_maxBrakeNotch;
        }

        // ----------------------------------------------------
        // 条件3: パターンおよび残り1〜5mにおける目標ノッチ計算
        // ----------------------------------------------------
        int targetTascBrake = 0;
        bool rampMode = false;            // ★追加: パターンに沿って減速中か(trueのとき、ブレーキを直線的に強める)
        float rampOver = 0.0f;            // ★追加: パターン速度に対する超過 (km/h)
        float rampDecelKmhps = 0.0f;      // ★追加: こう配補正後の目標減速度 (km/h/s)

        if (remainingDistance > 0.0f) {
            float currentSpeedMS = currentSpeed / 3.6f;
            float delayDistance = currentSpeedMS * TASC_DELAY_TIME_SEC;
            float effectiveDistance = remainingDistance - delayDistance;

            if (effectiveDistance <= 0.0f) {
                targetTascBrake = effectiveMaxNotch;
            }
            else {
                // 基本パターン速度の計算 (v = sqrt(2a_d))
                                // こう配補正: のぼりは減速しやすく、くだりは減速しにくい
                float avgGrade = GetAverageGrade(currentLocation, s_targetLocation);
                float decel = TASC_DECEL_MS2 + avgGrade * GRADE_ACCEL_MS2_PER_PERMILLE;
                float minDecel = TASC_DECEL_MS2 * 0.3f;   // くだりがきつくても、減速度に下限を設ける
                if (decel < minDecel) decel = minDecel;
                float targetSpeedMS = sqrtf(2.0f * decel * effectiveDistance);
                float targetSpeedKMH = targetSpeedMS * 3.6f;

                if (!isnan(targetSpeedKMH)) {
                    float overSpeed = currentSpeed - targetSpeedKMH;
                    rampMode = true;                  // ★追加
                    rampOver = overSpeed;             // ★追加
                    rampDecelKmhps = decel * 3.6f;    // ★追加
                    // 速度偏差に応じたノッチ計算（滑らかな線形補正）
                    // 1ノッチあたりの速度差感度（7段換算で約1.0km/h = 1ノッチ）
                    float notchRatio = (float)effectiveMaxNotch / 7.0f;
                    float overSpeedGain = 1.0f / notchRatio;

                    if (overSpeed > 0.0f) {
                        // パターン超過時：超過量に応じてブレーキを強くする
                        targetTascBrake = (int)(overSpeed / overSpeedGain) + (int)(effectiveMaxNotch * 0.5f);
                    }
                    else {
                        // パターン以下時：-1.0km/hまでは現在のノッチを維持（不感帯）して揺らぎを防ぐ
                        if (overSpeed >= -TASC_DEADBAND_KMH && s_currentTascBrake > 0) {
                            targetTascBrake = s_currentTascBrake;
                        }
                        else {
                            targetTascBrake = (int)(overSpeed / overSpeedGain) + (int)(effectiveMaxNotch * 0.5f);
                        }
                    }

                    if (remainingDistance <= TASC_FINAL_DIST_M) {
                        // 残り5m以下では最小ノッチを下限規制（例: 全ノッチの25%未満には落とさない）
                        int minNotchAt5m = (int)(effectiveMaxNotch * TASC_FINAL_MIN_RATIO);
                        if (minNotchAt5m < 2) minNotchAt5m = 2;

                        // オーバーラン危険時（速度が速い）でなければ極端に下げない
                        if (targetTascBrake < minNotchAt5m && overSpeed > -2.0f) {
                            targetTascBrake = minNotchAt5m;
                        }
                    }

                    // 範囲クランプ
                    if (targetTascBrake > effectiveMaxNotch) targetTascBrake = effectiveMaxNotch;
                    if (targetTascBrake < 0) targetTascBrake = 0;
                }
            }
        }
		else {
			targetTascBrake = effectiveMaxNotch; // まだ動いてる場合は最大ノッチで止める
		}

        // ----------------------------------------------------
        // 条件4: ノッチ変化制御（ランプアップ ＆ ノッチスキップ）
        // ----------------------------------------------------
        int notchDiff = abs(targetTascBrake - s_currentTascBrake);
        if (rampMode && remainingDistance > TASC_FINAL_DIST_M) {
            float aTarget = rampDecelKmhps + ((rampOver > 0.0f) ? TASC_RAMP_OVER_GAIN : TASC_RAMP_UNDER_GAIN) * rampOver;
            float aMin = rampDecelKmhps * 0.3f;
            if (aTarget < aMin) aTarget = aMin;
            float aPred = s_tascDecelKmhps + s_tascDecelTrend * TASC_DECEL_LEAD_SEC;
            float over = rampOver + TASC_RAMP_MARGIN_KMH;
            bool active = (s_currentTascBrake > 0) ? (over > -TASC_DEADBAND_KMH) : (over > 0.0f);

            if (s_currentTascBrake == 0) {
                s_notchTimerMs = 0;
                if (active) {
                    // 初回: 目標減速度に必要なノッチを一度に出す
                    int n = (int)(aTarget / s_tascKPerNotch + 0.5f);
                    if (n < 1) n = 1;
                    if (n > effectiveMaxNotch) n = effectiveMaxNotch;
                    s_currentTascBrake = n;
                }
            }
            else {
                s_notchTimerMs += deltaTimeMs;
                if (absSpeed < TASC_LOW_SPEED_KMH) {
                    if (s_notchTimerMs >= 200) {
                        if (over > 0.5f && s_currentTascBrake < effectiveMaxNotch) {
                            s_currentTascBrake++;
                            s_notchTimerMs = 0;
                        }
                        else if (over < -TASC_DEADBAND_KMH && s_currentTascBrake > 1) {
                            // パターンより遅い分だけ、まとめて緩める(最大4段)
                            int down = 1 + (int)((-over - TASC_DEADBAND_KMH) / 0.5f);
                            if (down > 4) down = 4;
                            s_currentTascBrake -= down;
                            if (s_currentTascBrake < 1) s_currentTascBrake = 1;
                            s_notchTimerMs = 0;
                        }
                    }
                }
                else if (s_notchTimerMs >= TASC_HOLD_MS) {
                    if (!active) {
                        // パターンより十分遅いので1段ずつ緩める
                        s_currentTascBrake--;
                        s_notchTimerMs = TASC_HOLD_MS - 300;
                    }
                    else {
                        // 1ノッチあたりの減速度を学習(安定しているときだけ)
                        if (s_currentTascBrake >= 3 && fabsf(s_tascDecelTrend) < 0.5f) {
                            float k = s_tascDecelKmhps / (float)s_currentTascBrake;
                            s_tascKPerNotch += (k - s_tascKPerNotch) * 0.3f;
                            if (s_tascKPerNotch < 0.03f) s_tascKPerNotch = 0.03f;
                            if (s_tascKPerNotch > 1.0f)  s_tascKPerNotch = 1.0f;
                        }
                        float err = aTarget - aPred;
                        if (fabsf(err) > TASC_ERR_TOL) {
                            int n = (int)(err / s_tascKPerNotch + (err > 0 ? 0.5f : -0.5f));
                            // 誤差が小さいときは1段ずつ(2段ずつ動くと段差ができる)
                            if (fabsf(err) < 0.5f) n = (err > 0) ? 1 : -1;
                            if (n > TASC_MAX_STEP) n = TASC_MAX_STEP;
                            if (n < -TASC_MAX_STEP) n = -TASC_MAX_STEP;
                            int next = s_currentTascBrake + n;
                            if (next < 1) next = 1;
                            if (next > effectiveMaxNotch) next = effectiveMaxNotch;
                            if (next != s_currentTascBrake) {
                                s_currentTascBrake = next;
                                s_notchTimerMs = 0;
                            }
                        }
                    }
                }
            }
        }

            else {            // 最終接近区間、または停止に間に合わない場合: 今までどおりの制御
            int notchDiff = abs(targetTascBrake - s_currentTascBrake);

            // ★ 偏差が2段以上ある場合、または残距離5m以内かつ偏差が大きい場合はノッチスキップ（タイマー無視で直接進める）
            if (notchDiff >= 2 || (remainingDistance <= TASC_FINAL_DIST_M && notchDiff >= 1)) {
                s_currentTascBrake = targetTascBrake;
                s_notchTimerMs = 0;
            }
            else if (s_currentTascBrake < targetTascBrake) {
                s_notchTimerMs += deltaTimeMs;
                if (s_notchTimerMs >= NOTCH_STEP_INTERVAL_MS) {
                    s_currentTascBrake++;
                    s_notchTimerMs = 0;
                }
            }
            else if (s_currentTascBrake > targetTascBrake) {
                s_notchTimerMs += deltaTimeMs;
                if (s_notchTimerMs >= NOTCH_STEP_INTERVAL_MS) {
                    s_currentTascBrake--;
                    s_notchTimerMs = 0;
                }
            }
            else {
                s_notchTimerMs = 0;
            }
        }

        // 5. 手動ノッチとTASC出力の比較・最終マッピング
        if (s_currentTascBrake > 0) {
            if (g_extendedNotches > 0) {
                // 拡張ノッチ出力インデックスにシフト
                int offset = g_maxBrakeNotch + 2;
                int mappedTascNotch = offset + s_currentTascBrake;

                // 手動ブレーキが勝っている場合は手動を優先
                return (driverBrake > mappedTascNotch) ? driverBrake : mappedTascNotch;
            }
            else {
                // 従来モード
                return (driverBrake > s_currentTascBrake) ? driverBrake : s_currentTascBrake;
            }
        }
        return driverBrake;
    }
    // 現在出力中のTASCノッチ段数（0〜14）を返す関数
    int GetCurrentTascBrakeNotch() {
        // TASC非作動・電源OFF時は 0 表示。インチング中も 0 表示
        if (g_TASCATOSet == 0 || !g_isAtcPowerOn || s_inchingActive || s_inchingBraking) {
            return 0;
        }
        if (!s_isTascActive && !s_holdActive) return 0;   // 停車保持ブレーキ中は、その段数を表示する
        return s_currentTascBrake;
    }
