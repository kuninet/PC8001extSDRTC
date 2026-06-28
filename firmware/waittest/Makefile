# waittest ファーム用 Makefile(PIC18F47Q43)
#
# ビルド  : MPLAB XC8(xc8-cc)。MPLAB X IDE は不要(コンパイラ単体でOK)。
# 書き込み: MPLAB SNAP + MPLAB IPE 付属の ipecmd(CLI)。
#           SNAP は LVP(低電圧書込)専用なので、ファーム側 config が LVP=ON
#           であること(waittest.c で設定済み)・ボードが自己給電(+5V)で
#           あることが前提。SNAP はターゲットに給電しないので通電状態で繋ぐ。
#
# 生成物は build/ にまとめて出す(リポジトリの .gitignore が build/ を除外済み
# なので、中間ファイルがソース直下を汚さない)。
#
# 主なターゲット:
#   make            build/waittest.hex を生成する
#   make flash      SNAP で PIC へ書き込む(ipecmd)
#   make clean      build/ を削除する
#
# 使い方の例(PATH に xc8-cc / ipecmd が無い場合は変数で指定):
#   make XC8=/Applications/microchip/xc8/v3.10/bin/xc8-cc
#   make flash IPECMD=/Applications/microchip/mplabx/v6.30/mplab_platform/mplab_ipe/bin/ipecmd

MCU    = 18F47Q43
TARGET = waittest
SRC    = waittest.c
BUILD  = build
HEX    = $(BUILD)/$(TARGET).hex

# ツール(PATH にあればそのまま。無ければフルパスを上書き指定)
XC8    ?= xc8-cc
IPECMD ?= ipecmd

# ipecmd ツールコード: SNAP=SN(PICkit3=PK3 / PICkit4=PK4)。
# 正確なフラグは IPE で一度 GUI 書込した際のログに出る ipecmd 行で確認して合わせる。
TOOL   ?= SN

# XC8 v3.x は DFP(デバイスサポートパック)の指定が必須。MPLAB X 同梱の
# PIC18F-Q_DFP の **xc8 サブフォルダ**を指すこと(DFP ルートだとエラー 2104)。
# バージョン番号は環境に合わせて上書きする:
#   make DFP=/Applications/microchip/mplabx/v6.30/packs/Microchip/PIC18F-Q_DFP/1.28.451/xc8
DFP    ?= /Applications/microchip/mplabx/v6.30/packs/Microchip/PIC18F-Q_DFP/1.28.451/xc8

CFLAGS = -mcpu=$(MCU) -mdfp=$(DFP) -O2

all: $(HEX)

# build/ 内でコンパイルし、中間ファイルも含めて生成物を build/ に閉じ込める。
$(HEX): $(SRC) | $(BUILD)
	cd $(BUILD) && $(XC8) $(CFLAGS) -o $(TARGET).hex ../$(SRC)

$(BUILD):
	mkdir -p $(BUILD)

# SNAP で書き込む。-M=プログラム。
# ターゲット電圧は基板の自己給電(+5V)に追従(SNAP は給電しない)。
flash: $(HEX)
	$(IPECMD) -TP$(TOOL) -P$(MCU) -M -F$(HEX)

clean:
	rm -rf $(BUILD)

.PHONY: all flash clean
