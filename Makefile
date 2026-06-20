# PC8001extSDRTC ビルド用 Makefile
#
# 前提:
#   - Java 実行環境(JAVA 変数で指定可)
#   - tools80.jar … PC-8001 用アセンブラ。SD-DOS リポジトリ同梱のものを使う。
#     既定は同階層に SD-DOS がある前提(../SD-DOS/tools/tools80.jar)。
#     別の場所なら `make TOOLS80=/path/to/tools80.jar` で指定する。
#   - (任意)ホスト C コンパイラ … firmware スタブの構文チェック用
#
# 主なターゲット:
#   make            PC-8001 側テスト(WAITTEST.cmt)を build/ に生成する
#   make waittest   同上(個別)
#   make fw-check   firmware の C スタブをホストコンパイラで構文チェックする
#   make clean      build/ を削除する
#
# PIC ファーム本体(firmware/*.c)の実機ビルドは MPLAB X + XC8 で行う(本 Makefile の対象外)。

JAVA    ?= java
TOOLS80 ?= ../SD-DOS/tools/tools80.jar
CC      ?= cc
BUILD    = build

ASM = printf 'OK\n' | $(JAVA) -jar $(TOOLS80) -tgt=z80

all: $(BUILD)/WAITTEST.cmt

waittest: $(BUILD)/WAITTEST.cmt

# PC-8001 側 /WAIT 検証プログラム(LOAD → G9000)
$(BUILD)/WAITTEST.cmt: test/WAITTEST.asm | $(BUILD)
	$(ASM) test/WAITTEST.asm
	mv test/WAITTEST.cmt $@

# firmware スタブの構文チェック(実機ビルドは MPLAB X / XC8)
fw-check:
	$(CC) -fsyntax-only -std=gnu99 -w -I. firmware/picsd_main.c
	$(CC) -fsyntax-only -std=gnu99 -w -I. firmware/waittest/waittest.c
	@echo "firmware: 構文OK(実機ビルドは MPLAB X / XC8)"

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: all waittest fw-check clean
