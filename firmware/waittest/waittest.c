/*
 * waittest.c - /WAIT ハンドシェイク検証用 PIC18F47Q43 スタブファーム
 *
 * 目的: PC-8001 拡張バスが /WAIT を尊重するか(PICが /WAIT を下げると Z80 が
 *       マシンサイクルを伸ばして待つか)を最小構成で確かめる。SD/RTC は不要。
 *
 * PC-8001側テスト: ../../test/WAITTEST.asm(LOAD → G9000)
 *
 * 契約(WAITTEST.asm と対):
 *   - テストポート = TEST_PORT(既定 D0H)。
 *   - 内部レジスタ last(電源投入時 = 0x55)。
 *   - READ(IN)  : /WAIT を Low → 意図的に遅延 → データバスに last を出す → /WAIT 解放。
 *   - WRITE(OUT): /WAIT を Low → 少し待って D0-D7 を取り込み last へ → /WAIT 解放。
 *
 * 判定の考え方:
 *   /WAIT が効いていれば、PIC をわざと遅くしても Z80 は待ってから読むので
 *   読み値は常に正しい(0x55)。効いていなければゴミ値になる。
 *
 * 本ファイルは骨格(スタブ)。実機ではバス信号の捕捉方法(CLC / 割り込み / ポーリング)、
 * ピン定義、/WAIT のアサート/解放、データバスの方向制御を実装する。
 * ピン割り当ては ../../hardware/design.md を参照。
 */

#include <stdint.h>

#define TEST_PORT   0xD0u
#define INIT_LAST   0x55u

/* 意図的な遅延量(/WAIT を下げている時間)。
 * まず大きめ(数µs相当)にして「遅くても正しく読めるか」を見る。
 * /WAIT が効くと確認できたら徐々に詰める。 */
#define WAIT_DELAY_LOOPS  200u

static volatile uint8_t last = INIT_LAST;

/* ---- ハード抽象(実機で実装) ---- */
static uint8_t  bus_addr(void);          /* A0-A7 を読む                    */
static uint8_t  bus_is_iorq_rd(void);    /* /IORQ & /RD アサート中か        */
static uint8_t  bus_is_iorq_wr(void);    /* /IORQ & /WR アサート中か        */
static uint8_t  bus_read_data(void);     /* D0-D7 を読む(WRITE時)         */
static void     bus_drive_data(uint8_t); /* D0-D7 を出力(READ時)          */
static void     bus_release_data(void);  /* D0-D7 をハイインピーダンスへ    */
static void     wait_assert(void);       /* /WAIT を Low(オープンドレイン)*/
static void     wait_release(void);      /* /WAIT を解放                    */

static void spin(uint16_t n) { while (n--) { __asm("nop"); } }

/* テストポートへのアクセス1回を処理する。
 * 実機では /IORQ 立ち下がりを CLC / 割り込みで捕捉してここへ入る。 */
static void handle_io(void)
{
    if (bus_addr() != TEST_PORT) {
        return;                 /* 対象ポート以外は無視 */
    }

    if (bus_is_iorq_rd()) {
        /* READ: 先に /WAIT を下げ、わざと遅れてからデータを出す */
        wait_assert();
        spin(WAIT_DELAY_LOOPS);
        bus_drive_data(last);
        wait_release();
        /* Z80 がラッチし切るまで保持してから解放する */
        bus_release_data();
    } else if (bus_is_iorq_wr()) {
        /* WRITE: /WAIT で整流してから取り込む */
        wait_assert();
        spin(WAIT_DELAY_LOOPS);
        last = bus_read_data();
        wait_release();
    }
}

void main(void)
{
    last = INIT_LAST;
    /* TODO: クロック(内部64MHz)、ポート方向、ODCON(/WAIT)、CLC/割り込み 初期化 */
    for (;;) {
        handle_io();            /* 実機では割り込み駆動に置き換え */
    }
}

/* ---- スタブ実装(TODO: 実機ピンに合わせて実装) ---- */
static uint8_t bus_addr(void)          { return 0; }
static uint8_t bus_is_iorq_rd(void)    { return 0; }
static uint8_t bus_is_iorq_wr(void)    { return 0; }
static uint8_t bus_read_data(void)     { return 0; }
static void    bus_drive_data(uint8_t v){ (void)v; }
static void    bus_release_data(void)  { }
static void    wait_assert(void)       { }
static void    wait_release(void)      { }
