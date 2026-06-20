/*
 * picsd_main.c - PC8001extSDRTC PIC18F47Q43 ファーム(スタブ)
 *
 * PC-8001 外部CPUバス直結。SDカードを Z80 の I/O デバイス(D0H-D6H)として見せる。
 * I/O プロトコルの真実は ../docs/protocol.md を参照。
 *
 * 本ファイルは設計段階の骨格です。実機ファームではバスI/O捕捉(CLC/割り込み)、
 * SD SPI、/WAIT 制御を実装します。
 */

#include <stdint.h>

/* ---- I/O ポート(Z80 side, D0H-D6H) ---- */
#define PORT_CMD    0xD0u   /* W  コマンド            */
#define PORT_ADR0   0xD1u   /* W  アドレス byte0(LSB)*/
#define PORT_ADR1   0xD2u   /* W  アドレス byte1      */
#define PORT_ADR2   0xD3u   /* W  アドレス byte2      */
#define PORT_ADR3   0xD4u   /* W  アドレス byte3(MSB)*/
#define PORT_DATA   0xD5u   /* R/W 512B データFIFO    */
#define PORT_STAT   0xD6u   /* R  ステータス          */

/* ---- コマンド ---- */
#define CMD_READ    0x00u
#define CMD_WRITE   0x01u
#define CMD_STATUS  0x02u
#define CMD_INIT    0x03u

/* ---- ステータスビット ---- */
#define ST_READY    0x01u
#define ST_BUSY     0x02u
#define ST_ERROR    0x80u

/* ---- 状態 ---- */
static uint8_t  sd_buf[512];   /* セクタバッファ            */
static uint16_t buf_ptr;       /* DATA ストリーム位置       */
static uint32_t sd_addr;       /* バイトアドレス(MMCADR相当)*/
static volatile uint8_t status;

/* ---- SD SPI(スタブ。実装は sd_spi.c へ分割予定) ---- */
static int  sd_init(void);                       /* CMD0/CMD8/ACMD41        */
static int  sd_read_block(uint32_t addr, uint8_t *dst);   /* CMD17           */
static int  sd_write_block(uint32_t addr, const uint8_t *src); /* CMD24      */

/* ---- コマンドディスパッチ ---- */
static void do_command(uint8_t cmd)
{
    switch (cmd) {
    case CMD_INIT:
        status = ST_BUSY;
        status = sd_init() == 0 ? ST_READY : (ST_READY | ST_ERROR);
        break;
    case CMD_READ:
        status = ST_BUSY;
        if (sd_read_block(sd_addr, sd_buf) == 0) {
            buf_ptr = 0;
            status = ST_READY;
        } else {
            status = ST_READY | ST_ERROR;
        }
        break;
    case CMD_WRITE:
        /* DATA へ512B受領後に sd_write_block() を呼ぶ。ここでは受領開始だけ。 */
        status = ST_BUSY;
        buf_ptr = 0;
        break;
    case CMD_STATUS:
    default:
        break;
    }
}

/* ---- バス I/O ハンドラ(スタブ) ----
 * 実機では /IORQ 立ち下がりを CLC/割り込みで捕捉し、A0-A7 と /RD,/WR で分岐する。
 * 必要な数百 ns だけ /WAIT をアサートして整流する(SDアクセス中は固めない)。
 */
void on_io_write(uint8_t port, uint8_t value)
{
    switch (port) {
    case PORT_CMD:  do_command(value); break;
    case PORT_ADR0: sd_addr = (sd_addr & 0xFFFFFF00u) | value; break;
    case PORT_ADR1: sd_addr = (sd_addr & 0xFFFF00FFu) | ((uint32_t)value << 8); break;
    case PORT_ADR2: sd_addr = (sd_addr & 0xFF00FFFFu) | ((uint32_t)value << 16); break;
    case PORT_ADR3: sd_addr = (sd_addr & 0x00FFFFFFu) | ((uint32_t)value << 24); break;
    case PORT_DATA:
        sd_buf[buf_ptr++] = value;
        if (buf_ptr >= 512u) {
            status = ST_BUSY;
            status = sd_write_block(sd_addr, sd_buf) == 0
                         ? ST_READY : (ST_READY | ST_ERROR);
        }
        break;
    default: break;
    }
}

uint8_t on_io_read(uint8_t port)
{
    switch (port) {
    case PORT_DATA:
        return (buf_ptr < 512u) ? sd_buf[buf_ptr++] : 0xFFu;
    case PORT_STAT:
        return status;
    default:
        return 0xFFu;
    }
}

/* ---- SD SPI スタブ実装(TODO) ---- */
static int sd_init(void)                                  { return 0; }
static int sd_read_block(uint32_t addr, uint8_t *dst)     { (void)addr; (void)dst; return 0; }
static int sd_write_block(uint32_t addr, const uint8_t *src){ (void)addr; (void)src; return 0; }

void main(void)
{
    status = 0;
    buf_ptr = 0;
    sd_addr = 0;
    /* TODO: クロック/CLC/SPI/I2C 初期化、バス I/O 割り込み許可 */
    for (;;) {
        /* バス I/O は割り込み駆動。本ループはアイドル/RTC ポーリング等。 */
    }
}
