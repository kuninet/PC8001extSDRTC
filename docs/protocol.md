# PC8001extSDRTC I/O プロトコル仕様(D0H-D6H)

PC-8001(Z80)と PIC18F47Q43 の間の **I/O ポート契約**です。SD-DOS の `src/MMC_PIC.asm` と
PIC ファームは、この仕様を共有の真実として実装します。

- 対象 I/O アドレス: **D0H-D6H**(PC-8001 未割り当ての拡張領域)
- アクセス: Z80 の `IN`/`OUT` 命令(8bit ポート I/O)
- 本仕様の版: **v0(暫定)** — 実機検証前。変更し得る。

## ポートマップ

| ポート | R/W | 名称 | 役割 |
|---|---|---|---|
| `D0H` | W | COMMAND | コマンド発行 |
| `D1H` | W | ADDR0 | セクタアドレス byte0(LSB) |
| `D2H` | W | ADDR1 | セクタアドレス byte1 |
| `D3H` | W | ADDR2 | セクタアドレス byte2 |
| `D4H` | W | ADDR3 | セクタアドレス byte3(MSB) |
| `D5H` | R/W | DATA | 512B セクタバッファのストリーム入出力(FIFO) |
| `D6H` | R | STATUS | ステータスレジスタ |

## コマンド(`D0H` に書く)

| 値 | 名称 | 動作 |
|---|---|---|
| `00H` | READ_SECTOR | ADDR のセクタを SD から PIC の512Bバッファへ読み込む。完了で `READY=1`。DATA 読み出しポインタを 0 に戻す。 |
| `01H` | WRITE_SECTOR | DATA へ512B受け取る準備をする。512バイト受領後、ADDR のセクタへ SD に書き戻す。フラッシュ完了で `READY=1`。 |
| `02H` | GET_STATUS | ステータスを更新する(明示フェッチ。副作用なし)。 |
| `03H` | INIT | SD カードを初期化/マウントする(SPIモード初期化、CMD0/CMD8/ACMD41 等)。成功で `READY=1`、失敗で `ERROR=1`。 |
| `10H`〜 | (将来)RTC | I2C RTC の読み書き(時計取得/設定)。本版では未定義。 |

## ステータスレジスタ(`D6H` を読む)

| bit | 名称 | 意味 |
|---|---|---|
| 0 | READY | 1 = 直前の操作が完了し、データ転送可 |
| 1 | BUSY | 1 = SD アクセス中 |
| 7 | ERROR | 1 = エラー発生(カード無し/CRC/タイムアウト等) |

その他のビットは予約(0 を返すこと)。

## アドレスのセマンティクス

ADDR0-3 は 32bit のアドレスで、**既存のビットバンギングドライバの `MMCADR0-3` と同一の値**です。
これは元の SDSC フロー(CMD17/CMD24)で使っていた **バイトアドレス**で、1セクタ=512バイト、
セクタを1つ進めるときは +200H(=512)します。

PIC 側はこの ADDR を同じ意味(バイトアドレス)で受け取り、SDHC のブロックアドレッシングを
使う場合は内部で 512 で割ってブロック番号に変換します。これにより SD-DOS の FS 層を無変更に
保てます。

## 転送シーケンス

### セクタ READ

```
1. OUT (D1H),ADDR0 ; … OUT (D4H),ADDR3   ; アドレスをセット
2. OUT (D0H),00H                          ; READ_SECTOR
3. wait until (IN (D6H) AND 01H) != 0     ; READY を polling(SDアクセス完了待ち)
4. 512回: IN (D5H) -> メモリ              ; DATA から1バイトずつ取り出す(PICが内部ポインタを自動前進)
```

### セクタ WRITE

```
1. OUT (D1H),ADDR0 ; … OUT (D4H),ADDR3   ; アドレスをセット
2. OUT (D0H),01H                          ; WRITE_SECTOR
3. 512回: メモリ -> OUT (D5H)             ; DATA へ1バイトずつ送る(PICが内部バッファに蓄積)
4. wait until (IN (D6H) AND 01H) != 0     ; READY を polling(フラッシュ完了待ち)
```

### 初期化(マウント)

```
1. OUT (D0H),03H                          ; INIT
2. wait until (IN (D6H) AND 01H) != 0     ; READY を polling
3. if (IN (D6H) AND 80H) != 0 -> エラー   ; ERROR ならカード無し等
```

## /WAIT ハンドシェイク

- PIC は **1バイトの DATA(`D5H`)転送とコマンド ラッチの数百 ns** の間だけ /WAIT をアサートして
  Z80 を整流する。
- **遅いSDアクセス(~0.5ms)の間は Z80 を /WAIT で固めない。** その間は STATUS を polling して待つ。
  これにより N-BASIC の周期割り込みを止めずに済む。
- 実機検証の最優先項目は「PC-8001 外部バスが /WAIT を物理的に尊重するか」。万一不可なら、
  Z80 側のアクセスを遅くする(NOP挿入)か、ステータス polling 前提のタイミングで回避する。

## SD-DOS 側エントリとの対応(参考)

`src/MMC_PIC.asm` がこのプロトコルを実装し、ビットバンギング版 `MMC.asm` と同名のエントリを
提供する(上位の FS.asm / STRM.asm は無変更)。

| SD-DOS エントリ | このプロトコルでの実装 |
|---|---|
| `MMC_INIT` | `OUT (D0H),03H` → READY 待ち → ERROR チェック |
| `MMC_BRD_CMD` | ADDR セット → `OUT (D0H),00H` → READY 待ち |
| `MMC_1RD` | `IN (D5H)` → C |
| `MMC_1WR` | `OUT (D5H),C` |
| `MMC_BRD_END` | 何もしない(PIC が CRC 処理済み) |
| `MMC_READ` | `MMC_BRD_CMD` → 512B `IN (D5H)` → アドレス +200H をブロック数ぶん |
| `MMC_WRITE` | ADDR セット → `OUT (D0H),01H` → 512B `OUT (D5H)` → READY 待ち |
| `MMC_INC_ADR` / `MMC_CLR_ADR` | `MMCADR0-3` の加算/クリア(ビットバンギング版と同一) |

## バージョン履歴

- v0(暫定): 初版。実機検証前の机上仕様。
