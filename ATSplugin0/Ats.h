// 以下の ifdef ブロックは DLL から簡単にエクスポートさせるマクロを作成する標準的な方法です。 
// この DLL 内のすべてのファイルはコマンドラインで定義された ATS_EXPORTS シンボル
// でコンパイルされます。このシンボルはこの DLL が使用するどのプロジェクト上でも未定義でなけ
// ればなりません。この方法ではソースファイルにこのファイルを含むすべてのプロジェクトが DLL 
// からインポートされたものとして ATS_API 関数を参照し、そのためこの DLL はこのマク 
// ロで定義されたシンボルをエクスポートされたものとして参照します。
//#pragma data_seg(".shared")
//#pragma data_seg()

#define ATS_BEACON_S 0 // Sロング
#define ATS_BEACON_SN 1 // SN直下
#define ATS_BEACON_SNRED 2 // SN誤出発防止
#define ATS_BEACON_P 3 // P停止信号
#define ATS_BEACON_EMG 4 // P即停(非常)
#define ATS_BEACON_SVC 5 // P即停(常用)
#define ATS_BEACON_SPDLIM 6 // P分岐器速度制限
#define ATS_BEACON_SPDMAX 7 // P最高速度
#define ATS_BEACON_SPP 8 // 停車駅通過防止装置

int g_emgBrake; // 非常ノッチ
int g_svcBrake; // 常用最大ノッチ
int g_brakeNotch; // ブレーキノッチ
int g_powerNotch; // 力行ノッチ
int g_reverser; // レバーサ
bool g_pilotlamp; // パイロットランプ
int g_time; // 現在時刻
float g_speed; // 速度計の速度[km/h]
int g_deltaT; // フレーム時間[ms/frame]

ATS_HANDLES g_output; // 出力

CAtssn g_atssn; // ATS-SN
CAtsp g_atsp; // ATS-P
CSpp g_spp; // 停車駅通過防止装置