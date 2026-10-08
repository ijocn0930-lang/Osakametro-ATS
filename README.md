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
速度超過すると、警告音が鳴り、ブレーキがかかります。停止信号を冒進した場合は、非常ブレーキがかかります。
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
信号インデックスは10以降です。10未満は境界駅でのバグ回避のため制限速度は無制限ですが、10未満の信号インデックスを指定しないでください。

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

値が小さすぎるとエラーの原因になるためおやめください。
```
[ATC]
ATCFail = true
FailParameter = 0.00000001 ;オーバーフロー回避のためアプリケーションを終了する
```


### TASC

このプラグインはTASC機能が内蔵されています。  
まじかんと様の```bve-autopilot.dll```とは異なります。  
このTASCは車両によっては安定しない場合があります。注意して使用してください。  
**自動空気ブレーキ、電磁直通ブレーキ車はTASC制御がうまく行かないので使用しないでください。**  
**全電気指令式電磁直通ブレーキまたは全電気指令式ブレーキ車にのみ使用してください。**  
**今回のバージョンでは速度制限や信号インデックスを読み取りません。TASC制御中に低い現示や速度制限が近づいてきたら手動でブレーキをかけてください。**

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


### ATO
**準備中です。ATOに切り替えることはできますが、自動発進や力行指令と制動指令は何もしません。**

### ATO故障
**準備中です。ATOは何もしないため、故障する意味がありません。**


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
|ATS_KEY_G(5)|TASC/ATO切り替え|TASC/ATOに切り替えます。**ATOは何もしません。**|
|ATS_KEY_H(6)|未使用||
|ATS_KEY_I(7)|未使用||
|ATS_KEY_J(8)|未使用||
|ATS_KEY_K(9)|未使用||
|ATS_KEY_L(0)|未使用||


### その他

[VehicleSpec]セクションはブレーキ設定とATC設定です。```MaxBrake```については今度削除予定です。  
```MaxBrake = 6``` 運転士のノッチの常用最大ブレーキの設定  
```AtcType = 2``` 車両が対応するATCの設定。0でWS-ATCのみ、1でCS-ATCのみ、2で両方。  

```
[VehicleSpec]
MaxBrake=6
AtcType=2
```

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

## Beacon
|BeaconData.Type|BeaconData.Signal/Distance|BeaconData.Optinal|備考|  
|-----|-----|-----|-----|  
|31|1|0(使用しない)|前方予告受信地上子[^1]|
|1030|0(使用しない)|距離(40000で400m先に停車位置)|停車位置地上子 cm単位|
|1031|0(使用しない)|50で前後50cm以内|停車許容範囲の設定 cm単位|
|1003|0(使用しない)|使用しない|ATO自動発進 **未実装**|
|1007|0(使用しない)|使用しない|速度制限識別 **未実装**|
|1008|0(使用しない)|使用しない|こう配補正 **未実装**|




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