# 论文编译

源稿为 [PaperDraft.tex](PaperDraft.tex)。从论文根目录执行：

```sh
python3 paper/build.py
```

需要标准 TeX Live / MacTeX 的 `pdflatex`，以及源码使用的 ctex、CJK、TikZ/PGF、newtx、geometry、amsmath、mathtools、booktabs、enumitem、microtype、fancyhdr 等包。无需 Tapas、Codex 桌面应用、其编译服务或项目外的配置文件。

脚本始终以本目录为编译工作目录，运行两遍 PDFLaTeX。新 PDF 和日志默认写入调用脚本时所在目录的 `tmp/external/paper/build/`，不会覆盖已有 [PaperDraft.pdf](PaperDraft.pdf)。需要将编译结果更新为该文件时执行：

```sh
python3 paper/build.py --publish
```

`--output` 可指定编译输出目录。源码和图 1 的 TikZ 定义均在本目录内，没有指向外层 Tapas 仓库的输入文件。
