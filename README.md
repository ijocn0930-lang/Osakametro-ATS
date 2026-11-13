# ATSplugin0(Osakametro ATSプラグイン)
## 注意：このプラグインだけでは動きません。車両データに配置しなければなりません。

#### 書きかけの項目があります。

必ずお読みください。

## このプラグインは何？
これは、Osaka MetroのATSを再現するためのプラグインです。  
```Ats.dll```をAts32フォルダに配置しておきます。  
Vehicle.txtに以下のように記述することで、Osaka MetroのATSを使用することができます。  
完全ではない場合があります。注意して使用してください。
```
Ats32 = Ats32\Ats.dll
```
**現在、64bit版はありません。** BVE5をご利用ください。  
もし、64bit版が追加された場合は以下のように記述してください。  
**ATSプラグインを強制的に64bit**へブリッジすることを想定していません。
```
Ats64 = Ats64\Ats.dll
```
ほかのプラグインと共用したい場合はRock_On様の```DetailManager.dll```を利用して下さい。  
書き方はここでは省略しますので、ご自身でお調べください。
キーの競合などについては考慮しておりませんので自己責任でご利用ください。

## プラグインの機能

### WS-ATC
WS-ATCは、地上信号機を用いたアナログ信号を使用します。信号機の色に応じて、速度制限を行います。  
速度超過すると警告音が鳴り、ブレーキがかかります。停止信号を冒進した場合は非常ブレーキがかかります。
近鉄モードに切り替えると95km/hまで運転可能です。  
```
Section.Begin(0, 1, 2, 3, 4); //0,25,40,50,70
```

### CS-ATC
千日前線や中央線などの一部の路線では、CS-ATCが使用されています。
CS-ATCに切り替えたら、車内信号に従って運転します。速度が超過するとブレーキがかかります。  
車内信号の速度が変化したらベルが鳴ります  
```
Section.Begin(10, 11, 15, 18, 20, 24); //02,01,25,40,50,70
```
信号インデックスは10以降です。10未満は大阪港駅などの境界駅でのバグ回避のため制限速度は無制限ですが、10未満の信号インデックスを指定しないでください。

### CS-ATC故障
このプラグインは、CS-ATCの故障を再現することができます。CS-ATCが故障すると、車内信号が消灯し、非常ブレーキがかかります。
CS-ATC故障の設定はINIファイルの[ATC]セクションで行います。
```
[ATC]
ATCFail = false
FailParameter = 0.0
```
既定では```false```と```0.0```です。

```
[ATC]
ATCFail = true
FailParameter = 0.1
```
とすることで、10%の確率で閉塞を通過するたびに故障イベントを発生させます。  
ただし、以下のように設定してもATCFailの設定は無視されます。
```
[ATC]
ATCFail = true
FailParameter = 0.0 ;0.0に設定されている場合はATCFailがfalseとなる。
```

以下のように値が小さすぎるとエラーの原因になるためおやめください。
```
[ATC]
ATCFail = true
FailParameter = 0.000000009 ;オーバーフロー回避のためアプリケーションを終了する
```


### TASC

このプラグインはTASC機能が内蔵されています。  
まじかんと様の```bve-autopilot.dll```とは異なります。  
このTASCは車両によっては安定しない場合があります。注意して使用してください。  
**自動空気ブレーキ、電磁直通ブレーキ車はTASC制御がうまく行かないので使用しないでください。**  
**全電気指令式電磁直通ブレーキまたは全電気指令式ブレーキ車にのみ使用してください。**  
**今回のバージョンでは速度制限や信号インデックスを読み取りません。TASC制御中に低い現示や速度制限が近づいてきたら手動でブレーキをかけてください。**  
停車位置からずれて止まったときは、インチングキー(ATS_KEY_I)を押している間だけ低速で動いて位置を直せます(ずれが```InchingMaxDist```以内のとき)。

TASC制御は拡張ブレーキノッチ指令を使用しますが、```ExtendedNotches```が0になっている場合はTASC制御が運転士のノッチを出力します。
```
[TASC]
ExtendedNotches = 31
PressureRates = 0.0, 0.03, 0.06, 0.09, 0.12, 0.15, 0.18, 0.21, 0.24, 0.27, 0.30, 0.33, 0.36, 0.39, 0.42, 0.45, 0.48, 0.51, 0.54, 0.57, 0.60, 0.63, 0.66, 0.69, 0.72 ,0.75, 0.78, 0.81, 0.84, 0.87, 0.9, 0.92, 0.95, 0.979, 1.00, 1.20
```
```ExtendedNotches```の数が```PressureRates```の範囲内であるか確認する必要があります。  
```ExtendedNotches```が```PressureRates```より超える数にした場合はインデックスが配列の境界外です。  
車両パラメーターファイルの```PressureRates```は昇順でなければなりません。
**運転士のノッチの後に続いて0.0~1.0の範囲です。**

### TASC故障
このプラグインはTASC故障の設定ができます。TASC地上子を通過するたびにTASC故障イベントが発生すると、非常ブレーキがかかります。
TASC/ATOを手動に切り替えるまでノッチは受け付けません。

```
[TASC]
Failure = false
FailureRate = 0.0
```
既定では```false```と```0.0```です。

### 転動防止(停車保持ブレーキ)
TASC/ATOモードで停車しているとき、車両が転がらないようにブレーキを掛け続けます。  
次のときに掛かります。
- TASCで停車位置に止まったとき
- TASC/ATOモードでドアが開いたとき
- シナリオ開始・駅ジャンプ・モード切替のあと、止まっているとき

次のときに緩みます。
- 運転士がブレーキを保持ブレーキ以上に強めたとき(いったん弱めてから強める必要があります)
- ATOが発進したとき
- 手動モードに切り替えたとき、ATC電源をOFFにしたとき
- 速度が5km/hを超えたとき
- INIの設定によって、ドアが開いたとき・閉まったとき

```
[TASC]
HoldBrakeRatio = 1.0
HoldReleaseOnDoorOpen = false
HoldReleaseOnDoorClose = false
HoldReleaseDelay = 0.0
```
|キー|内容|既定値|
|-----|-----|-----|
|HoldBrakeRatio|保持ブレーキの強さ。最大ノッチに対する割合(0.1〜1.0)|1.0|
|HoldReleaseOnDoorOpen|trueでドアが開いたら緩める|false|
|HoldReleaseOnDoorClose|trueでドアが閉まったら緩める|false|
|HoldReleaseDelay|運転士がブレーキを操作してから緩めるまでの秒数(0〜30)|0.0|

### ATO
ATOモードでは、ATC信号、前方予告(Type31)、速度制限(1007)、こう配(1008)に合わせて、力行とブレーキを自動で操作します。駅ではTASCが停車位置に止めます。  
発進は、発進キー(ATS_KEY_H)か、自動発進地上子(1003)を通過したあとにドアが閉まったときです。発進するには、運転士のノッチがN、逆転器が前である必要があります。  
運転士がブレーキを掛けると、ATOは発進待ちに戻ります。

```
[ATO]
PowerNotch = 3
ExtendedPowerNotches = 0
DriverPowerMap =
AutoStartAfterJump = true
```
|キー|内容|既定値|
|-----|-----|-----|
|PowerNotch|ATOが出す力行ノッチ。拡張ノッチを使うときは拡張ノッチの段数で指定|3|
|ExtendedPowerNotches|力行の拡張ノッチ数(0で使わない)|0|
|DriverPowerMap|運転士のP1, P2, … をATOの何ノッチにするか(カンマ区切り)。書かなければ比例で割り当て|なし|
|AutoStartAfterJump|trueで、駅ジャンプ・シナリオ開始のあとドアが閉まったら自動発進する|true|
|PowerNotchReturn|falseで、戻しノッチに対応していない車両として扱う(力行中はノッチを下げず、下げるときはNに戻す。例: P4→P1はP4のまま、P4→NはN)|true|
|CutMargin|許容速度の何km/h手前で力行を切るか|5.0|
|ResumeMargin|許容速度の何km/h手前まで下がったら力行を再開するか|8.0|
|BrakeMargin|許容速度の何km/h手前を超えたらブレーキを掛けるか|3.0|
|Deceleration|減速パターンの減速度(km/h/s)|2.0|
|StopMargin|停止信号の何m手前で止まるか|10.0|
|GradeLookahead|こう配を先読みする距離(m)|100.0|
|CoastLookaheadSec|惰行したときの速度変化を先読みする時間(秒)|5.0|
|UphillNotchBonusMax|のぼりで力行ノッチを上げる最大段数(10‰ごとに1段)|2|

#### 力行の拡張ノッチ
運転士のノッチより細かい力行ノッチ(例: 運転士P1〜P4、ATO P1〜P7)を使えます。  
車両パラメーターファイルの```PowerNotchCount```と```PowerText```は運転士のノッチ数のままにして、電流・引張力・進段のテーブルはATOのノッチ数分を書きます。運転士のノッチは```DriverPowerMap```で変換されます。
```
[ATO]
ExtendedPowerNotches = 7
DriverPowerMap = 1, 3, 5, 7
```

### ATO故障
発進するたびに故障の抽選を行います。故障すると、ATOは動かなくなり、ATO故障灯が点灯します。
```
[ATO]
Failure = false
FailureRate = 0.0
```


### キーアサイン

|キー|処理|備考|  
|-----|-----|-----|
|ATS_KEY_S(Space)|未使用||
|ATS_KEY_A1(Insert)|ATC電源|マスコンキーの投入とほぼ同じ|
|ATS_KEY_A2(Delete)|未使用||
|ATS_KEY_B1(Home)|ATC非設|車庫内で使用します|
|ATS_KEY_B2(End)|未使用||
|ATS_KEY_C1(PageUp)|保安装置切り替え|```AtcType = 2```のみ|
|ATS_KEY_C2(PageDown)|非常運転|WS/CSが故障したときに信号を無視して運転できるようにします。|
|ATS_KEY_D(2)|号線戻し|号線切り替え|
|ATS_KEY_E(3)|号線進み|号線切り替え|
|ATS_KEY_F(4)|近鉄切り替え|近鉄モードに切り替えます。|
|ATS_KEY_G(5)|TASC/ATO切り替え|手動→TASC→ATO→手動の順。INIで使えないようにしたモードは飛ばします|
|ATS_KEY_H(6)|ATO発進|ATOモードで発進待ちのとき|
|ATS_KEY_I(7)|インチング|押している間だけ動きます|
|ATS_KEY_J(8)|未使用||
|ATS_KEY_K(9)|未使用||
|ATS_KEY_L(0)|未使用||


### その他

[VehicleSpec]セクションはブレーキ設定とATC設定です。```MaxBrake```については今度削除予定です。  
```MaxBrake = 6``` 運転士のノッチの常用最大ブレーキの設定  
```AtcType = 2``` 車両が対応するATCの設定。0でWS-ATCのみ、1でCS-ATCのみ、2で両方。  
```TascEnable = true``` 車両がTASCモードを使えるか。falseでTASCモードを飛ばします。  
```AtoEnable = true``` 車両がATOモードを使えるか。falseでATOモードを飛ばします。  
TascEnable と AtoEnable を書かなければ、両方とも使えます。ATOモードで駅に止まるときはTASCの制御を使うため、```TascEnable = false```でもATOモードでは定点停止します。

```
[VehicleSpec]
MaxBrake=6
AtcType=2
TascEnable=true
AtoEnable=true
```
[LineSpec]セクションは号線設定です。   
```DefalutLine = 4``` シナリオを読み込むときに設定されるデフォルトの号線   
```MinLine = 1``` 設定できる最小の号線  
```MaxLine = 8``` 設定できる最大の号線


## パネル出力

|パネル|機能|セクション|備考|    
|-----|-----|-----|-----|  
|1|非設|PilotLamp|非設モード|
|2|ATC電源|PilotLamp||
|3|デバッグ用|||
|4|保安装置切り替え|DitigalNumber|0:Null 1:WS 2:CS|
|5|号線インデックス|DigitalNumber|非設モード|
|6|デバッグ用|PilotLamp|号線不一致|
|7|デバッグ用|PilotLamp|構内モード|
|9|近鉄モード|PilotLamp||
|10~29|ATC信号|PilotLamp|10~12は0km/h信号|
|31|開通状態|PilotLamp|開通|
|32|開通状態|PilotLamp|停止|
|33|前方予告|PilotLamp|Beacon必須|
|50|ATC常用|PilotLamp||
|51|ATC非常|PilotLamp||
|53|ATC故障|PilotLamp|ATC故障したとき。```ATCFail = true```で有効|
|54|非常運転モード|PilotLamp|速度制限なしで運転できる。ただし故障したときだけに使用すること|
|133|TASC/ATO切り替え表示|DigitalNumber|0:ATC電源OFF 1:手動 2:TASC 3:ATO|
|134|TASC制御|PilotLamp|TASC停車位置地上子を踏むと点灯する|
|135|TASC制動指令|DigitalNumber|TASCモードのみ。0:非表示 1:N 2以降:B1〜(転動防止ブレーキも表示)|
|136|定点停止|PilotLamp|定点停止地上子で設定された許容範囲内に入ると点灯する。|
|137|ATO状態|DigitalNumber|0:非表示 1:発進待ち 2:ATO運転中|
|138|ATO力行指令|DigitalNumber|ATOモードの力行中のみ。0:非表示 2以降:P1〜|
|139|ATO制動指令|DigitalNumber|ATOモードで力行中以外。0:非表示 1:N 2以降:B1〜(TASC・転動防止ブレーキも表示)|
|140|ATO故障|PilotLamp||
|141|自動発進待機|PilotLamp|自動発進の待ちのとき点灯|
|142|インチング中|PilotLamp||
|40|可動柵開|PilotLamp|ホームドアのある駅でドアが開いている間。番号はINIの [PlatformDoor] PanelOpen で変更可|
|200〜239|現在駅・次駅|DigitalNumber|号線ごとに4つ(下り現在・下り次・上り現在・上り次)。値は駅番号(0:非表示)。INIの [StationDisplay] StationPanelStart で変更可|
|240〜243|キロ程|DigitalNumber|10km・1km・100m・10mの桁。0:非表示 1〜10:数字0〜9。INIの [StationDisplay] KiloPanelStart で変更可|
|197|母線引き通し|PilotLamp|OnSpeed(既定5km/h)以上で点灯。M車の集電靴が1つでもサードレールのない区間(地上子1044)に入ると消灯。番号はINIの [BusLine] Panel で変更可|

138と139は、どちらか一方だけを表示するので、同じ場所に画像を置けます。DigitalNumberの画像は「0コマ目＝空白、1コマ目＝N、2コマ目＝1段目…」の並びにしてください。

## サウンド
|サウンド|機能|備考|    
|-----|-----|-----|
|0|WS速度超過警報音|けたたましいビー音|
|2|CS信号更新音|ベル音|
|-|||





## 信号インデックス

|信号インデックス|WS-ATC|近鉄|CS-ATC|備考|  
|-----|-----|-----|-----|-----|  
|0|0km/h|0km/h|0km/h||
|1|25km/h|0km/h|0km/h||  
|2|40km/h|0km/h|0km/h||
|3|50km/h|0km/h|0km/h||
|4|70km/h|0km/h|0km/h||
|5|15km/h|15km/h|0km/h|終着駅用|
|6|0km/h|25km/h|0km/h||
|7|0km/h|40km/h|0km/h||
|8|0km/h|70km/h|0km/h||
|9|0km/h|95km/h|0km/h||
|10|0km/h|0km/h|0km/h|02信号|
|11|0km/h|0km/h|0km/h|01信号|
|12|0km/h|0km/h|0km/h|01信号|
|13|0km/h|0km/h|15km/h||
|14|||20km/h||
|15|||25km/h||
|16|||30km/h||
|17|||35km/h||
|18|||40km/h||
|19|||45km/h||
|20|||50km/h||
|21|||55km/h||
|22|||60km/h||
|23|||65km/h||
|24|||70km/h||
|25|||75km/h||
|26|||80km/h||
|27|||85km/h||
|28|||90km/h||
|29|||95km/h||
|30|||||
|31|||||
|32|||||
|33|||||
|34|||||
|~~|||||
|43|||25km/h|構内モード(未使用)|

CS-ATC信号インデックスにある10は02信号、11と12は01信号です。02信号は非常ブレーキ、01信号は常用ブレーキです。  
**現在、OsakaMetroはアナログATCのため、5km/h刻みの現示はありませんが、将来5km/h刻みの現示に対応するための準備をしてあります。**  
通常では0km/h,15km/h,25km/h,35km/h,40km/h,50km/h,60km/h,70km/h,95km/hのみ使用することをおすすめします。  
互換性保持のため、今度100以降の信号インデックスにアナログATCの実装を予定しているかもしれません。

## ホームドア(可動柵)連動パネル
ホームドアのある駅で、車両のドアが開くとホームドアも開き、車両のドアが閉まったあとにホームドアが閉まる動きを表示します。「可動柵開」のパネルは、ホームドアが閉まりきるまで点灯します。開閉の音も鳴らせます。  
ホームドアの絵を動かすことや、「ホームドアが開いてから車両のドアが開く」順番は、ATSプラグインではできません(BveEx側で行います)。  
駅ごとのホームドアの有無は、地上子1040で設定します。**ホームドアのない駅にも、0の地上子を置いてください**(次の1040を通るまで前の設定が残るため)。
```
[PlatformDoor]
Enable = true
DefaultStation = false
PanelOpen = 40
OpenDelay = 0
OpenTime = 3
CloseDelay = 1
CloseTime = 3
SoundOpen = -1
SoundClose = -1
PanelState = -1
```
|キー|内容|既定値|
|-----|-----|-----|
|Enable|車両がホームドア連動に対応しているか。falseで常に消灯|true|
|DefaultStation|シナリオ開始・駅ジャンプのあと、1040を通るまでのホームドアの有無|false|
|PanelOpen|可動柵開のパネル番号(0〜255)|40|
|OpenDelay|車両のドアが開いてから、ホームドアが開き始めるまで(秒)|0|
|OpenTime|ホームドアが開き終わるまで(秒)|3|
|CloseDelay|車両のドアが閉まってから、ホームドアが閉まり始めるまで(秒)|1|
|CloseTime|ホームドアが閉まり終わるまで(秒)|3|
|SoundOpen|開き始めるときの音のサウンド番号(-1で鳴らさない。0〜2は使用済み)|-1|
|SoundClose|閉まり始めるときの音のサウンド番号|-1|
|PanelState|ホームドアの状態のパネル番号(-1で使わない)。0:閉 1:開いている途中 2:開 3:閉まっている途中|-1|

## 分けた画像の速度計(8192pxを超えるDigitalNumber)
BVEの画像は8192pxを超えると、環境によって間延びなどが起きます。そこで、縦に長い速度計の画像(例: 256x16384、1km/hごと)を、GaugeImages\SplitStrip.ps1 で8192px以下の何枚かに分け、同じ場所に重ねて表示します。
```
.\SplitStrip.ps1 -Source speedm.png -Interval 98 -Name speedm

[SplitMeter]
Panel = 180     ; 最初のパネル番号(ここから Parts 個)。-1か書かなければ使わない
Parts = 3       ; SplitStrip.ps1 が表示する値
PerPart = 82
```
- cfg では、分けた画像を同じ Location に置き、Subject を ats180、ats181、… にして、後の画像ほど Layer を大きくする
- 速度は BVE の kmph と同じく小数点以下を切り捨て

## 母線引き通し表示灯
速度が OnSpeed(既定 5 km/h)以上で、M車の集電靴が全部サードレールに触れているときに点灯します(doorLock.dll の「一定速度以上で点灯」を引き継ぎ)。  
サードレールのない区間(絶縁・分岐器)は地上子1044で知らせます。M車の集電靴が1つでもその区間に入ると消灯し、最後の集電靴が出ると点灯します。
```
Beacon.Put(1044, 0, 1500);   // ここから15mはサードレールなし

[BusLine]
Enable = true
Panel = 197
OnSpeed = 5                              ; この速度(km/h)以上で点灯。0で速度に関係なく点灯
ShoePositions = 20.5, 37.5, 40.5, 57.5   ; M車の集電靴の位置(先頭からの距離m)。書かなければ先頭の位置で判断
```
- 止まったまま届いた1044(駅ジャンプの直後など)は使いません
- 停電区間(地上子1046)では消灯します

## 現在駅・次駅・キロ程の表示
地上子1042で現在駅と次駅を、地上子1043でキロ程を設定します。どちらも停車位置ちょうどに置くと、駅ジャンプのあとも表示されます。
```
Beacon.Put(1042, 0, 5006);    // 下り: 現在駅 5、次駅 6
Beacon.Put(1042, 0, -6005);   // 上り: 現在駅 6、次駅 5 (負の値が上り)
Beacon.Put(1043, 0, 11340);   // 下り: キロ程(m) − 距離程(m)
```
- 駅番号は1〜999。0は「なし」(非表示)
- 駅の表示は、選択中の号線と方向(上り・下り)のパネルだけに出します。号線1は200〜203、号線2は204〜207、…、号線10は236〜239
- 1043の値は、下りは「キロ程(m) − 距離程(m)」、上りは「キロ程(m) ＋ 距離程(m)」。上り・下りは1042の符号で決まります
- キロ程と距離程の差が変わらない区間なら、1043は1つでよい

```
[StationDisplay]
Enable = true
StationPanelStart = 200
KiloPanelStart = 240
```
|キー|内容|既定値|
|-----|-----|-----|
|Enable|表示するか。falseで常に非表示|true|
|StationPanelStart|駅表示の最初のパネル番号(ここから40個)|200|
|KiloPanelStart|キロ程の最初のパネル番号(ここから4個)|240|

## パネルの更新間隔・反映遅延
パネルごとに、表示を変える間隔と遅れをINIで設定できます。書いていないパネルは毎フレーム変わります。
```
[PanelTiming]
; パネル番号 = 更新間隔(ms), 反映遅延(ms)
0 = 500, 0      ; 0.5秒ごとに表示を変える
135 = 0, 300    ; 0.3秒遅れて表示する
138 = 200, 300  ; 0.3秒遅らせて、0.2秒ごとに表示を変える
```
- 更新間隔: 前に表示を変えてから、この時間がたつまで表示を変えない
- 反映遅延: 元の値が変わってから、この時間がたってから表示に出す
- 32個まで。両方0の行は無視する。DigitalNumber以外のパネルにも使える
- シナリオ開始・駅ジャンプのあとは、すぐに今の値を表示する

## 縦バー形の計器(BC圧・MR圧・MM電流・架線電圧)
上下に動くマーカーとバー、デジタル数字を出します。[Gauge1]〜[Gauge8] の8個まで設定できます。
```
[Gauge1]
Source = bc        ; bc / mr / er / bp / sap (kPa)、current (A)、voltage (架線電圧V)、control (制御電圧V)
Min = 0            ; 目盛りの下端
Max = 600          ; 目盛りの上端
Steps = 94         ; バーの段数(1段1pxなら目盛りの高さのpx)
Segments = 2       ; バーを縦に何枚の画像に分けるか
MarkerHalf = 2     ; マーカーの高さの半分(px)
BarPanel = 160     ; バーのパネル番号(ここから Segments 個。下から順)
DigitPanel = 151   ; 数字の最上位の桁のパネル番号(ここから Digits 個)
Digits = 3

[LineVoltage]      ; 架線電圧は ATSプラグインに渡されないので、電流から計算する
Nominal = 750
DropPerAmp = 0.05  ; 力行1Aあたりの電圧降下(V)
RisePerAmp = 0.03  ; 回生1Aあたりの電圧上昇(V)
Min = 650
Max = 800
Fluctuation = 10          ; 停車中も含めたゆらぎの大きさ(±V)。0でゆらがない
FluctuationPeriod = 3.0   ; 次のゆらぎの目標値を選ぶまでの平均の秒数(0.5〜1.5倍でばらつく)
SubstationRise = 30       ; 変電所の真横での電圧の上がり(V)
SubstationRange = 2000    ; 変電所の効きが届く距離(m)。力行・回生での上下は、真横で0.5倍〜遠いと1.5倍

[RegenFail]               ; 回生失効: 回生ブレーキ中に架線電圧(計算値)が Limit を超えたら、ブレーキを緩める・力行する・止まるまで電気ブレーキを止める(空気ブレーキだけになる)
Enable = true
Limit = 790               ; この架線電圧(V)を超えたら回生失効
Panel = -1                ; 回生失効の表示灯のパネル番号(-1で使わない)

[ControlVoltage]          ; 制御電圧(Source = control)。ATSプラグインに渡されないので、設定値にゆらぎを足して出す
Nominal = 100             ; ふだんの電圧(V)
Fluctuation = 1.0         ; ゆらぎの大きさ(±V)。0でゆらがない
FluctuationPeriod = 5.0
Min = 0
Max = 150
```
- 数字: 0=非表示 1〜10=数字0〜9。上の桁の0は非表示
- バーの画像は GaugeImages\MakeGauge.ps1 で作る(満・空の絵とマーカーの絵から、コマを縦に並べた画像を作り、ATS.ini と cfg の書き方も表示する)
- BVE の画像は幅・高さを2のべき乗にし、8192pxを超えないこと(MakeGauge.ps1 は自動で2のべき乗にする)
- 分けた画像は、つなぎ目の横線を防ぐため上下で2行ずつ重ねてある。cfg の Layer は上の画像ほど大きくする(例: seg0 = 20、seg1 = 21)
- プラグインと画像はコマの数え方をそろえてあるので、プラグインを更新したら画像も MakeGauge.ps1 で作り直す
- 400系(OC400)の設定例: BC圧 160〜161、MR圧 162〜163、MM電流 164〜165、架線電圧 166〜167(バー)。数字は BC圧 151〜153、MR圧 154〜157、MM電流 170〜173、架線電圧 174〜176

## WS/CS混合区間
WS-ATCモードではCS信号(10以降)を無視して、直前のWS信号を使い続けます。WS信号をまだ受けていないとき(CS区間でWSモードに切り替えた直後など)は、停止(0km/h)扱いになり非常ブレーキが掛かります。  
CS-ATCモードではWS信号(0〜9)を無視して、直前のCS信号を使い続けます。

## 信号インデックスの注意点
始発駅の後ろにSection.BeginまたはBeginNewがないとWS/CSいずれも0信号を受信して**非常ブレーキ**がかかってしまいます。
```
0; Section.begin(0, 1, 2, 3, 4); または(10, 15, 18, 20, 24);  //始発駅の後ろに閉塞を置く
100; Sta['Sta0'].Put(1, -2, 2); //始発駅
```
原因は、
```cpp:
default:g_wsLimitSpeed = 0; brake;
```
このように設定されており、閉塞がないと0km/hとみなすためです。

## Beacon
|BeaconData.Type|BeaconData.Signal/Distance|BeaconData.Optinal|備考|  
|-----|-----|-----|-----|  
|31|1|0(使用しない)|前方予告受信地上子[^1]|
|1030|0(使用しない)|距離(40000で400m先に停車位置)|停車位置地上子 cm単位|
|1031|0(使用しない)|50で前後50cm以内|停車許容範囲の設定 cm単位|
|1003|0(使用しない)|使用しない|ATO自動発進。通過後にドアが閉まると自動発進する|
|1007|0(使用しない)|距離(m)×1000＋制限速度(km/h)。999で解除|速度制限(ATO用)。例: 600060で600m先から60km/h|
|1008|0(使用しない)|±(距離(m)×1000＋こう配(‰))。負はくだり|こう配補正(TASC・ATO用)|
|1040|0(使用しない)|1:ホームドアあり 0:なし|次に止まる駅のホームドアの有無。駅の手前に置く|
|1041|0(使用しない)|許容範囲(cm)。0で既定値|始発駅・駅ジャンプのあとの定点停止灯(136)。定点停止の駅の停車位置ちょうどに置く。止まったまま受けるとATC電源が入っていれば点灯し、許容範囲を超えて動くと消灯する。走行中に受けたときは何もしない|
|1042|0(使用しない)|現在駅×1000＋次駅。負の値は上り|現在駅・次駅の表示。停車位置に置く|
|1043|0(使用しない)|下り: キロ程(m)−距離程(m) 上り: キロ程(m)＋距離程(m)|キロ程の表示|
|1046|0(使用しない)|1:ここから停電 0:ここで復旧|停電区間。停電中は架線電圧0V・電流計0A・母線引き通し表示灯消灯・力行しない。INIの [Outage] CutRegen = true(既定)のときは、レバーサーを中立にして出し、電気ブレーキを止める(空気ブレーキだけになる)|
|1045|0(使用しない)|変電所の距離程(m)|架線電圧の計算用。変電所に近いほど電圧が高くなる。位置を直接書くので、どこに置いてもよい|
|1044|0(使用しない)|区間の長さ(cm)|サードレールのない区間(絶縁・分岐器)の始まりに置く。母線引き通し表示灯用。走行中に受けたときだけ使う|




### ライセンス
本プラグインは**MIT License**のもとで公開されています。
著作権者の提示とライセンス条文の同梱だけで誰にも改造、流用することができます。
ただし、車両データに本プラグインが同梱された場合は車両データ側のライセンスが優先されます。

---
MIT License

Copyright (c) 2026 ijocn0930-lang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.






[^1]:メトロ総合プラグインと互換性があります。信号が変化すると前方予告が消えてしまうため、大量の地上子を設置してください。