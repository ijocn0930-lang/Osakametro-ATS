#include "Tasc.h"
#include <math.h>
#include <vector>
#include <stdlib.h>


// --- 設定値・制御変数 ---
static std::vector<float> s_pressureRates;          // 圧力比率テーブル

static int s_currentTascBrake = 0;                  // 現在のTASC内部ノッチ (0 〜 s_extendedNotches)
static int s_lastTimeMs = -1; // 前回のフレーム時刻 (ms)
static int s_notchTimerMs = 0; // ノッチ変化タイマー (ms)
static float s_targetLocation = -1.0f;   // 目標停止位置 (m)
static float s_stopTolerance = 0.5f;     // 許容範囲 (m) 既定値: ±50cm (0.5m)
static float s_pendingRelativeDist = -1.0f;

static const float TASC_DECEL_MS2 = 0.6944f;        // 目標減速度 (2.5 km/h/s)
static const float TASC_DELAY_TIME_SEC = 1.0f;     // 空走時間 (秒)
static const int NOTCH_STEP_INTERVAL_MS = 100;     // 100msランプアップ

void InitTasc() { // TASC内部状態の初期化
    s_isTascActive = false;
    s_targetLocation = -1.0f;
    s_pendingRelativeDist = -1.0f;
    s_stopTolerance = 0.5f;
    s_currentTascBrake = 0;
    s_lastTimeMs = -1;
    s_notchTimerMs = 0;
    g_PositionEnable = false;
}

bool IsTascActive() { // TASC作動中フラグの取得関数
    return s_isTascActive;
}


// 内部計算用の最大ノッチ段数を取得
static int GetEffectiveMaxNotch() {
    return (g_extendedNotches > 0) ? g_extendedNotches : g_maxBrakeNotch;
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
            s_stopTolerance = 0.5f;
        }
    }
}
// ドアが開いたらTASCをリセットする
void OnDoorOpen() {
    s_isTascActive = false;
    s_targetLocation = -1.0f;
    s_pendingRelativeDist = -1.0f;
    s_currentTascBrake = 0;
    s_lastTimeMs = -1;
    s_notchTimerMs = 0;
    g_PositionEnable = false;
}

// TASCブレーキ演算処理
int CalculateTascBrake(float currentLocation, float currentSpeed, int driverBrake, int currentTimeMs) {
    if (currentLocation <= 0.0f) return driverBrake;

    int deltaTimeMs = 0;
    if (s_lastTimeMs >= 0) {
        deltaTimeMs = currentTimeMs - s_lastTimeMs;
        if (deltaTimeMs < 0) deltaTimeMs = 0;
    }
    s_lastTimeMs = currentTimeMs;

    // 地上子データの同期処理
    if (s_pendingRelativeDist > 0.0f) {
        s_targetLocation = currentLocation + s_pendingRelativeDist;
        s_isTascActive = true;
        s_pendingRelativeDist = -1.0f;
        s_currentTascBrake = 0;
        g_PositionEnable = false;
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
            s_currentTascBrake = effectiveMaxNotch; // 即座に最大ノッチ化（タイマースキップ）
            s_notchTimerMs = 0;

            if (g_extendedNotches > 0) {
                int offset = g_maxBrakeNotch + 2;
                return offset + s_currentTascBrake;
            }
            return g_maxBrakeNotch;
            if (s_currentTascBrake < driverBrake) {
				s_currentTascBrake = 0;
				return driverBrake;
            }
        }
        // ② 停車直前の微低速域 (0.5km/h未満) -> 衝動緩和のためブレーキを弱める (B1~B2相当)
        else if (absSpeed < 0.5f) {
            int releaseNotch = (int)(effectiveMaxNotch * 0.2f); // 全体の約20%（例: 14段中B2-B3、7段中B1）
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

        if (remainingDistance > 0.0f) {
            float currentSpeedMS = currentSpeed / 3.6f;
            float delayDistance = currentSpeedMS * TASC_DELAY_TIME_SEC;
            float effectiveDistance = remainingDistance - delayDistance;

            if (effectiveDistance <= 0.0f) {
                targetTascBrake = effectiveMaxNotch;
            }
            else {
                // 基本パターン速度の計算 (v = sqrt(2a_d))
                float targetSpeedMS = sqrtf(2.0f * TASC_DECEL_MS2 * effectiveDistance);
                float targetSpeedKMH = targetSpeedMS * 3.6f;

                if (!isnan(targetSpeedKMH)) {
                    float overSpeed = currentSpeed - targetSpeedKMH;

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
                        if (overSpeed >= -1.0f && s_currentTascBrake > 0) {
                            targetTascBrake = s_currentTascBrake;
                        }
                        else {
                            targetTascBrake = (int)(overSpeed / overSpeedGain) + (int)(effectiveMaxNotch * 0.5f);
                        }
                    }

                    if (remainingDistance <= 5.0f) {
                        // 残り5m以下では最小ノッチを下限規制（例: 全ノッチの25%未満には落とさない）
                        int minNotchAt5m = (int)(effectiveMaxNotch * 0.25f);
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

        // ----------------------------------------------------
        // 条件4: ノッチ変化制御（ランプアップ ＆ ノッチスキップ）
        // ----------------------------------------------------
        int notchDiff = abs(targetTascBrake - s_currentTascBrake);

        // ★ 偏差が2段以上ある場合、または残距離5m以内かつ偏差が大きい場合はノッチスキップ（タイマー無視で直接進める）
        if (notchDiff >= 2 || (remainingDistance <= 5.0f && notchDiff >= 1)) {
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
        // TASC非作動・電源OFF時は 0 表示
        if (!s_isTascActive || g_TASCATOSet == 0 || !g_isAtcPowerOn) {
            return 0;
        }
        return s_currentTascBrake;
    }

