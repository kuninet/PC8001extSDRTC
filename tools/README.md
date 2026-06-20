# tools/

ビルドに使う外部ツールを置くディレクトリ。ツール本体は再配布しないため、リポジトリには
含めない(この README 以外は `.gitignore` 対象)。

## tools80.jar

PC-8001 用アセンブラ(j80 付属)。

* 入手元: OUT of STANDARD http://upd780c1.g1.xrea.com/pc-8001/index.html の `bin/tools80_r6_50.lzh`
* `.lzh` を展開し、`tools80.jar` をこのディレクトリ(`tools/tools80.jar`)に置く
* r6_50(Ver 6.6.68)で動作確認済み。r6_44 以前は `SET` で始まるラベルがエラーになるため使わないこと
* Java 実行環境が必要。macOS では Homebrew の `openjdk` で動作確認済み

置いたら、リポジトリ直下で `make` するとビルドできる:

```sh
make            # build/WAITTEST.cmt を生成
```

別の場所の jar を使う場合は `make TOOLS80=/path/to/tools80.jar`。
