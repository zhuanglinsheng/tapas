#!/usr/bin/env python3
"""Compile the paper with ordinary PDFLaTeX; no desktop app or Tapas required."""
import argparse
from pathlib import Path
import shutil
import subprocess

HERE=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('tmp')/'external'/'paper'/'build')
    parser.add_argument('--publish',action='store_true',help='Also replace the PDF next to the TeX source')
    args=parser.parse_args()
    engine=shutil.which('pdflatex')
    if not engine:parser.error('pdflatex is required (TeX Live with ctex, CJK, TikZ and newtx).')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    log=out/'compile.log'
    with log.open('w') as stream:
        for _ in range(2):
            command=[engine,'-no-shell-escape','-interaction=nonstopmode','-halt-on-error','-file-line-error',
                     '-output-directory='+str(out),str(HERE/'PaperDraft.tex')]
            run=subprocess.run(command,cwd=HERE,stdout=stream,stderr=subprocess.STDOUT)
            if run.returncode:
                raise SystemExit('PDFLaTeX failed; see '+str(log))
    pdf=out/'PaperDraft.pdf'
    if not pdf.is_file():raise SystemExit('Compiler produced no PDF; see '+str(log))
    if args.publish and pdf!=HERE/'PaperDraft.pdf':shutil.copyfile(pdf,HERE/'PaperDraft.pdf')
    print(pdf)
    return 0


if __name__=='__main__':raise SystemExit(main())
