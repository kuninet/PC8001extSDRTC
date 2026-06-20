# firmware — PIC18F47Q43 ファーム(設計メモ)

MPLAB X + XC8 を想定。現状は構成と関数スタブのみ(`picsd_main.c`)。

## 役割

PC-8001 からの I/O アクセス(`D0H`-`D6H`)を受け、SD カードの生512Bブロック R/W を行う。
プロトコルは [../docs/protocol.md](../docs/protocol.md) が真実。

## 構成(予定)

```
firmware/
├── README.md
└── picsd_main.c     I/Oプロトコル処理 + SD SPI(生ブロックR/W) + /WAIT ハンドシェイク のスタブ
```

将来 `sd_spi.c` / `rtc_i2c.c` / `bus_io.c` に分割する。

## 主な処理ブロック

1. **バス I/O 受付**: CLC + 割り込み(または /IORQ ポーリング)で `IN`/`OUT` を捕捉。
   - `OUT D0H`: コマンドラッチ → 状態機械を起動。
   - `OUT D1H-D4H`: アドレスレジスタへ格納。
   - `OUT D5H`: 書き込みバッファへ蓄積(512で WRITE 実行)。
   - `IN  D5H`: 読み出しバッファから1バイト返す(ポインタ前進)。
   - `IN  D6H`: ステータスを返す。
2. **SD SPI**: 初期化(CMD0/CMD8/ACMD41)、CMD17(READ_SINGLE)、CMD24(WRITE_SINGLE)。
3. **/WAIT 制御**: 1バイト整流の数百 ns のみアサート。SDアクセス中は固めない(ステータス polling)。

## ステータスビット

`READY=bit0` / `BUSY=bit1` / `ERROR=bit7`。詳細は protocol.md。
