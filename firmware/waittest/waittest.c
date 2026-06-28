/*
 * waittest.c - /WAIT ハンドシェイク検証用 PIC18F47Q43 ファーム
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
 * ピン割り当て(../../hardware/design.md と一致):
 *   /IORQ = RA0(in) / /RD = RA1(in) / /WR = RA2(in) / /RESET = RA3(in)
 *   /WAIT = RA4(out, オープンドレイン) … 母板側プルアップで Wired-OR
 *   A0-A7 = RC0-RC7(in, PORTC) / D0-D7 = RD0-RD7(bidir, PORTD)
 *   クロック = 内部 64MHz HFINTOSC(水晶不要)
 *
 * 方式: 割り込みではなく単純ポーリング(最小テストのため)。/IORQ がアサート
 *   されたら自テストポート宛か即デコードし、自分宛なら /WAIT を下げて整流する。
 *   他ポート宛は一切 /WAIT を触らない(機械全体を止めないため)。
 *
 * ビルド: MPLAB X + XC8(PIC18F47Q43)。本ファイルはホストの構文チェック
 *   (`make fw-check`)も通るよう、PIC固有部を __XC8 でガードしている。
 */

#include <stdint.h>

#ifdef __XC8
#include <xc.h>

/* ===== コンフィグレーションビット(PIC18F47Q43) ===== */
// CONFIG1
#pragma config FEXTOSC = OFF         /* 外部発振子は未使用            */
#pragma config RSTOSC = HFINTOSC_64MHZ /* リセット直後から内部64MHz   */
// CONFIG2
#pragma config CLKOUTEN = OFF
#pragma config PR1WAY = ON
#pragma config CSWEN = ON
#pragma config FCMEN = ON
// CONFIG3
#pragma config MCLRE = EXTMCLR       /* MCLR リセット回路を使う        */
#pragma config PWRTS = PWRT_OFF
#pragma config MVECEN = OFF
#pragma config IVT1WAY = ON
#pragma config LPBOREN = OFF
#pragma config BOREN = SBORDIS
// CONFIG4
#pragma config BORV = VBOR_1P9
#pragma config ZCD = OFF
#pragma config PPS1WAY = OFF
#pragma config STVREN = ON
#pragma config LVP = ON
#pragma config XINST = OFF
// CONFIG5
#pragma config WDTCPS = WDTCPS_31
#pragma config WDTE = OFF             /* ウォッチドッグ無効            */
// CONFIG6
#pragma config WDTCWS = WDTCWS_7
#pragma config WDTCCS = SC
// CONFIG7
#pragma config BBSIZE = BBSIZE_512
#pragma config BBEN = OFF
#pragma config SAFEN = OFF
#pragma config DEBUG = OFF
// CONFIG8
#pragma config WRTB = OFF
#pragma config WRTC = OFF
#pragma config WRTD = OFF
#pragma config WRTSAF = OFF
#pragma config WRTAPP = OFF
// CONFIG9
#pragma config CP = OFF

#define _XTAL_FREQ 64000000UL
#endif /* __XC8 */

#define TEST_PORT   0xD0u
#define INIT_LAST   0x55u

/* 意図的な遅延量(/WAIT を下げている時間)。
 * まず大きめ(数µs相当)にして「遅くても正しく読めるか」を見る。
 * /WAIT が効くと確認できたら徐々に詰める。 */
#define WAIT_DELAY_LOOPS  200u

static volatile uint8_t last = INIT_LAST;

/* ---- ハード抽象 ---- */
static void     hw_init(void);           /* クロック/ポート/ODCON 初期化     */
static uint8_t  bus_iorq_active(void);   /* /IORQ アサート中か               */
static uint8_t  bus_addr(void);          /* A0-A7 を読む                    */
static uint8_t  bus_rd_active(void);     /* /RD アサート中か                */
static uint8_t  bus_wr_active(void);     /* /WR アサート中か                */
static uint8_t  bus_read_data(void);     /* D0-D7 を読む(WRITE時)         */
static void     bus_drive_data(uint8_t); /* D0-D7 を出力(READ時)          */
static void     bus_release_data(void);  /* D0-D7 をハイインピーダンスへ    */
static void     wait_assert(void);       /* /WAIT を Low(オープンドレイン)*/
static void     wait_release(void);      /* /WAIT を解放                    */

static void spin(uint16_t n)
{
    while (n--) {
#ifdef __XC8
        NOP();
#endif
    }
}

/* テストポートへのアクセス1回を処理する。
 * /IORQ がアサートされてから自ポート宛かを判定し、自分宛のときだけ /WAIT を使う。 */
static void handle_io(void)
{
    if (!bus_iorq_active()) {
        return;                 /* I/O サイクルでない */
    }
    if (bus_addr() != TEST_PORT) {
        return;                 /* 自テストポート以外は無視(/WAIT も触らない) */
    }

    if (bus_rd_active()) {
        /* READ: 先に /WAIT を下げ、わざと遅れてからデータを出す */
        wait_assert();
        spin(WAIT_DELAY_LOOPS);
        bus_drive_data(last);
        wait_release();
        /* Z80 がラッチし切る(/RD 解放)までデータを保持してから解放する */
        while (bus_rd_active()) { }
        bus_release_data();
    } else if (bus_wr_active()) {
        /* WRITE: /WAIT で整流してから取り込む */
        wait_assert();
        spin(WAIT_DELAY_LOOPS);
        last = bus_read_data();
        wait_release();
    }

    /* サイクル完了(/IORQ 解放)まで待ち、同一アクセスの二重処理を防ぐ */
    while (bus_iorq_active()) { }
}

void main(void)
{
    last = INIT_LAST;
    hw_init();
    for (;;) {
        handle_io();
    }
}

/* ===== ハード実装 ===== */
#ifdef __XC8

/*
 * RA0=/IORQ RA1=/RD RA2=/WR RA3=/RESET RA4=/WAIT(OD)
 * RC0-7=A0-A7  RD0-7=D0-D7
 * 制御線・アドレスはアクティブ Low(アサート=0)。
 */
static void hw_init(void)
{
    /* 内部 64MHz を明示設定(RSTOSC で既定だが念のため) */
    OSCFRQ = 0x08;              /* HFFRQ = 64 MHz */
    OSCCON1 = 0x00;             /* NDIV=1(分周なし) */

    /* 全ピン デジタル(アナログ機能オフ) */
    ANSELA = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELB = 0x00;
    ANSELE = 0x00;

    /* PORTA: RA0-RA3 入力(制御線)、RA4 出力(/WAIT) */
    TRISA = 0xEF;              /* 1110_1111: RA4 のみ出力 */
    ODCONA = 0x10;            /* RA4 をオープンドレインに */
    LATAbits.LATA4 = 1;       /* /WAIT 解放(Hi-Z=母板プルアップで High) */

    /* PORTC: A0-A7 入力 */
    TRISC = 0xFF;

    /* PORTD: D0-D7 既定は入力(Hi-Z)。READ 時のみ出力に切り替える */
    TRISD = 0xFF;
    LATD = 0x00;

    /* ICSP(RB6/RB7)はアプリ未使用。内部弱プルアップでフロート回避 */
    WPUBbits.WPUB6 = 1;
    WPUBbits.WPUB7 = 1;
}

static uint8_t bus_iorq_active(void)  { return PORTAbits.RA0 == 0; }
static uint8_t bus_addr(void)         { return PORTC; }
static uint8_t bus_rd_active(void)    { return PORTAbits.RA1 == 0; }
static uint8_t bus_wr_active(void)    { return PORTAbits.RA2 == 0; }
static uint8_t bus_read_data(void)    { return PORTD; }

static void bus_drive_data(uint8_t v)
{
    LATD = v;
    TRISD = 0x00;             /* 出力へ */
}

static void bus_release_data(void)
{
    TRISD = 0xFF;             /* 入力(Hi-Z)へ戻す */
}

static void wait_assert(void)  { LATAbits.LATA4 = 0; }  /* OD: Low ドライブ */
static void wait_release(void) { LATAbits.LATA4 = 1; }  /* OD: Hi-Z 解放    */

#else  /* !__XC8 : ホスト構文チェック用ダミー */

static void    hw_init(void)            { }
static uint8_t bus_iorq_active(void)    { return 0; }
static uint8_t bus_addr(void)           { return 0; }
static uint8_t bus_rd_active(void)      { return 0; }
static uint8_t bus_wr_active(void)      { return 0; }
static uint8_t bus_read_data(void)      { return 0; }
static void    bus_drive_data(uint8_t v){ (void)v; }
static void    bus_release_data(void)   { }
static void    wait_assert(void)        { }
static void    wait_release(void)       { }

#endif /* __XC8 */
