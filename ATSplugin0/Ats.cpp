#include <windows.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include "atsplugin.h"
#include "Tasc.h"
#include <stdio.h>

// --- 外部ファイル（CsAtc.cpp）にある関数を宣言 ---
extern int GetCsAtcSpeed(int signal);

// --- 地上子タイプ定義 ---
const int BEACON_TYPE_LINE_CHECK = 20; // 号線照合用地上子のType番号
const int BEACON_TYPE_ADVANCE_NOTICE = 31;  // 前方予告用地上子 (21 -> 31へ修正)

// --- グローバル変数 ---
 // 現在の閉塞から取得したATC制限速度 (km/h)
int g_currentAtcSpeed = 0;   // 現在の閉塞から取得したATC制限速度 (km/h)

int g_lastRawSignal = 0;     // BVEから最後に送られてきた生の信号インデックスを記憶する

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
int g_maxPowerNotch = 0;
int g_atsReverserNotch = 0;
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
int g_ATOPower = 0; //ATO力行指令　0で運転士のノッチと同じ　INIファイル対応　ATC電源がオフの時は非表示
int g_ATOStart = 0; //ATO発進するかどうか。ATOがオフの時は-1。0で手動、1で自動　ATC電源がオフの時は非表示
bool g_inching = false; //オーバーランしたときにインチングします。（インチングはキー押下のみ手動）
const int BEACON_TASC_POSITION = 1030; //停車位置の設定　TASC制御地上子含む　センチメートル単位
const int BEACON_TASC_POSITIONRANGE = 1031; //許容範囲 センチメートル単位
const int BEACON_LEVEL = 1008; //こう配補正
const int BEACON_ATOAUTOSTART = 1003; //自動発進
const int BEACON_SPEEDLIMIT = 1007; //速度制限
bool s_isTascActive = false; //TASC制御中かどうか
int g_extendedNotches = 0; //拡張ノッチ段数　INIファイル対応
int g_pressureRatesCount = 0; //圧力比率テーブルの要素数

// このDLL自体のインスタンスハンドルを保持する変数
HINSTANCE g_hModule = NULL;
// 号線変更に伴う内部フラグの更新
// 号線番号がけいはんな線や北急線独自割り当て番号（例: 近鉄=10, 北急=9 等）の運用に応じた処理
    // ※ 乗り入れ境界での手動切り替え時は、号線番号に応じて路線属性を確定させます
    // 必要に応じて号線番号の割り当て条件を調整可能です

void UpdateLineFlagsFromCurrentLine() {
    g_isMidosuji = (g_currentLine == 1);
    g_isChuo = (g_currentLine == 4);
    g_isKitakyu = (g_currentLine == 9);
    // 号線を直接切り替えた場合はけいはんな線モードを解除
    g_isKeihan = false;
}
// CS-ATC保持状態のリセット関数
void ResetCsAtcState() {
    g_csLimitSpeed = 0;
    g_lastCsLimitSpeed = -1;
    g_isAdvanceNoticeActive = false;
}
// カンマ区切りの文字列から要素数をカウントする関数
int CountCommaSeparatedElements(const char* str) {
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
// DLLのエントリポイント　INIファイルから読み込む車両の仕様設定
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) {
        char dllPath[MAX_PATH];
        char iniPath[MAX_PATH];

        GetModuleFileNameA(hinstDLL, dllPath, MAX_PATH);

        char* lastSlash = strrchr(dllPath, '\\');
        if (lastSlash != NULL) {
            *lastSlash = '\0';
            wsprintfA(iniPath, "%s\\ATS.ini", dllPath);
        }
        else {
			wsprintfA(iniPath, ".\\ATS.ini"); //リリース時はATS.iniがDLLと同じフォルダにあることを想定
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

        // --- 【新規】号線仕様の読み込み ---
        g_minLine = GetPrivateProfileIntA("LineSpec", "MinLine", 1, iniPath);
        g_maxLine = GetPrivateProfileIntA("LineSpec", "MaxLine", 8, iniPath);
        g_defaultLine = GetPrivateProfileIntA("LineSpec", "DefaultLine", 1, iniPath);

        // 範囲チェックとバリデーション
        if (g_minLine < 1) g_minLine = 1;
        if (g_maxLine > 8) g_maxLine = 8;
        if (g_minLine > g_maxLine) g_minLine = g_maxLine;

		// デフォルト号線が範囲外の場合は、最小号線または最大号線に補正
        if (g_defaultLine < g_minLine) g_defaultLine = g_minLine;
        if (g_defaultLine > g_maxLine) g_defaultLine = g_maxLine;

        // 構内モード制限速度の読み込み (デフォルト 25km/h)
        g_yardLimitSpeed = GetPrivateProfileIntA("AtcSpec", "YardLimitSpeed", 25, iniPath);
        if (g_yardLimitSpeed <= 0) g_yardLimitSpeed = 25;

        // --- ATC故障パラメータの読み込み ---
        char failBuf[32] = { 0 };
        GetPrivateProfileStringA("ATC", "ATCFail", "false", failBuf, sizeof(failBuf), iniPath);
        if (_stricmp(failBuf, "true") == 0 || atoi(failBuf) == 1) {
            g_atcFailSetting = true;
        }
        else {
            g_atcFailSetting = false;
        }

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
            return FALSE;
        }
    }
    return TRUE;
}
// プラグインのバージョンを返す関数 return 値は ATS_VERSION を返す それ以外は無効 ビルドエラーになる可能性があるので、必ず ATS_VERSION を返すこと
ATS_API int WINAPI GetPluginVersion()
{
	return ATS_VERSION; //0x00020000を返す
}

// プラグインがロードされるとき
ATS_API void WINAPI Load()
{
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
    g_ATOPower = 0;
    g_Position = 0;
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


    // 現在選択されているアクティブなATCモード (false = WS-ATC, true = CS-ATC) 初期化
    if (g_atcTypeSetting == 1) {
        g_isCurrentModeCS = true;
    }
    else {
        g_isCurrentModeCS = false;
    }
    g_lastLocation = -1.0f; // 初期位置判定用にリセット
    // 号線を初期値にリセット
    g_currentLine = 4; // 中央線初期化
    g_isLineMismatch = false; // 初期化

    // 前方予告作動中フラグを初期化
    g_isAdvanceNoticeActive = false;
    // 号線変更に伴う内部フラグの更新
    UpdateLineFlagsFromCurrentLine();
    g_isInNagataStation = false;
    g_isChuo = true;
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
    // 信号インデックスが変更された場合、前方予告を自動解除
    if (signal != g_lastRawSignal) {
        g_isAdvanceNoticeActive = false;
    }
	// 受信した信号インデックスを記憶する
    g_lastRawSignal = signal;

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

        // D. 複数号線データの判定 (バグ回避用に改修)
        else if (beaconValue > 8) {
            // 方式1: ビットマスク判定 (例: 1 << 10 など)
            if (g_currentLine <= 30 && (beaconValue & (1 << g_currentLine)) != 0) {
                isMatched = true;
            }
            else {
                // 方式2: 数値桁解析（「104」のような10進数結合データに対応）
                // 2桁番号（10など）と1桁番号が混在しても確実に判定する
                int temp = beaconValue;
                while (temp > 0) {
                    // 下2桁が一致するか（例: 1045 のうち 10）
                    if (temp % 100 == g_currentLine) {
                        isMatched = true;
                        break;
                    }
                    // 下1桁が一致するか（例: 45 のうち 5）
                    if (temp % 10 == g_currentLine) {
                        isMatched = true;
                        break;
                    }
                    temp /= 10;
                }
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
			if (beaconData.Signal < g_lastRawSignal) {
				g_isAdvanceNoticeActive = true; // 前方予告作動中フラグを立てる
			}
			else {
				g_isAdvanceNoticeActive = false; // 信号に変化ない時前方予告解除フラグを立てる
			}
		}
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
            SetSignal(g_lastRawSignal);
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
            SetSignal(g_lastRawSignal);
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
            SetSignal(g_lastRawSignal);
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
            else {
                // 近鉄けいはんな線モード -> 中央線モード復帰
                g_isKeihan = false;
                g_isChuo = true;
                g_isCurrentModeCS = (g_atcTypeSetting == 1 || g_atcTypeSetting == 2);
            }
            SetSignal(g_lastRawSignal);
        }
    }
    // =========================================================
    // TASC/ATO モード切替キー (ATS_KEY_C2)
    // =========================================================
    if (key == ATS_KEY_G) {
        // ATC電源がONかつ安全条件を満たしている場合のみ切替を許可
        if (g_isAtcPowerOn && isSafetyConditionMet) {
            g_TASCATOSet++;

            // 0: 手動 -> 1: TASC -> 2: ATO -> 0: 手動... と循環
            if (g_TASCATOSet > 2) {
                g_TASCATOSet = 0;
            }

            // モード切替時にTASCの内部状態をリセット
            InitTasc();
        }
    }

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
	// --- 0. 運転士のハンドル位置を一時変数にコピー ---
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
        // 2. TASCブレーキ計算の呼び出し
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
        // =========================================================
        // TASC自動ブレーキ制御判定 (ATC非常ブレーキ時のみ)
        // =========================================================

        else if (!g_isAtcEmergencyBrake) {
            int tascBrake = CalculateTascBrake(vehicleState.Location, vehicleState.Speed, outputBrake, currentTimeMs);

            // ATC常用ブレーキまたはTASCブレーキの厳しい方を採用
            if (tascBrake > outputBrake) {
                outputBrake = tascBrake;
            }
        }

        // 運転士ノッチによる通常の出力（制限速度内での運転）を許可
        else {
            outputBrake = g_driverBrake;
            outputPower = g_driverPower;
            if (g_isCurrentModeCS) {
                g_isAtcEmergencyBrake = false;
                g_isAtcServiceBrake = false;
            }
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

        bool isAtcSignalReceived = ((g_lastRawSignal >= 10 && g_lastRawSignal <= 30) || g_lastRawSignal == 43);

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
    // --- 非設モード警報音 (sound[2]) 制御 ---
        // 条件: 非設モード有効 かつ ATC信号インデックスを受信中(10〜30 または 43)
		// case 43 構内モード専用のため、非設モード警報音の条件に含める必要がある
        bool isAtcSignalReceived = ((g_lastRawSignal >= 10 && g_lastRawSignal <= 30) || g_lastRawSignal == 43);

        if (g_isOverrideMode && isAtcSignalReceived) {
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
    
    

	// --- 4. ハンドル出力の設定 ---
    handles.Power = outputPower;
    handles.Brake = outputBrake;
    handles.Reverser = g_driverReverser;
    handles.ConstantSpeed = 0;


    // 1. TASCブレーキ計算処理呼び出し
    int tascBrakeOutput = CalculateTascBrake(vehicleState.Location, vehicleState.Speed, g_driverBrake, vehicleState.Time);
    // ----------------------------------------------------
    // 2. パネル出力処理 (INI使わず定数値で直接指定)
    // ----------------------------------------------------

    // TASCブレーキ指令段数 (DigitalNumber: 0〜14)
    if (PANEL_TASC_NOTCH >= 0 && PANEL_TASC_NOTCH < 256) {
        panel[PANEL_TASC_NOTCH] = GetCurrentTascBrakeNotch();
    }

    // 定点停止灯 (PilotLamp: 0 または 1)
    if (PANEL_POSITION_ENABLE >= 0 && PANEL_POSITION_ENABLE < 256) {
        panel[PANEL_POSITION_ENABLE] = g_PositionEnable ? 1 : 0;
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

    // --- panel.txt への出力 ---
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
		panel[133] = 0; // TASC/ATOモード切替状態 (0:手動, 1:TASC, 2:ATO)
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
            panel[33] = g_isAdvanceNoticeActive ? 1 : 0; // 前方予告灯
            panel[50] = g_isAtcServiceBrake ? 1 : 0;   // ATC常用 (0:非表示, 1:点灯)
            panel[51] = g_isAtcEmergencyBrake ? 1 : 0; // ATC非常 (0:非表示, 1:点灯)
            // 号線不一致の場合：非常/絶対停止 (panel[10]) を消灯し、開通表示灯は消灯
        if (g_isLineMismatch) {
            panel[0] = 0;
            panel[51] = 1; // ATC非常 (0:非表示, 1:点灯)
            panel[g_lastRawSignal] = 0; // 車内信号灯は消灯
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
        if (g_isAtcFailed) {
            panel[51] = g_isAtcEmergencyBrake ? 1 : 0;
            panel[50] = 0;   // ATC常用 (0:非表示, 1:点灯)

        }
    }
    if (g_shouldPlayBell) {
        sound[1] = ATS_SOUND_PLAY; // 1番のインデックスに単発再生命令（1）を送る
        g_shouldPlayBell = false;  // 鳴らしたら即座にフラグを戻す（1フレームだけ鳴らすため）
    }

    return handles;
}

// その他の必須イベント関数（空実装）この関数がないとビルドエラーになるので、空実装で定義しておく
ATS_API void WINAPI KeyUp(int key) {}
ATS_API void WINAPI DoorOpen()
{
    OnDoorOpen();
}
ATS_API void WINAPI DoorClose() {}

// 駅ジャンプしたときやシナリオをやり直すときに呼ばれる関数（初期化処理）
ATS_API void WINAPI Initialize(int key)
{
    g_speed = 0.0f; //速度が0になるため、誤作動対策でこのコードを記述する
	if (g_isAtcPowerOn) {
		g_isAtcPowerOn = false; // ATC電源をOFFにする
		g_lastRawSignal = -1; // 信号インデックスを初期化（未受信状態）
	}
	InitTasc(); // TASCの内部状態をリセット
	if (g_isInNagataStation) {
		g_isInNagataStation = false; // 長田駅エリアフラグをリセット
	}
	g_isLineMismatch = false; // 号線不一致フラグをリセット
	// 注意1：ATCFaildフラグはリセットしない（故障状態を保持するため）
	// 注意2: g_isCurrentModeCS はリセットしない（現在のモードを保持するため）
	// 注意3: g_isKeihan はリセットしない（現在のモードを保持するため）
	// 注意4: g_isYardMode はATC電源OFF時に自動的に解除されるため、ここではリセットしない
	// 注意5: g_isOverrideMode はATC電源OFF時に自動的に解除されるため、ここではリセットしない

}
ATS_API void WINAPI HornBlow(int key) {}
