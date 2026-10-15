"""Build the Beamer showcase with two required pdfLaTeX passes."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    pdflatex = shutil.which('pdflatex')
    if pdflatex is None:
        parser.exit(1, 'The doc target requires pdflatex, Beamer and PGF/TikZ. '
                    'Install texlive-latex-recommended and texlive-pictures.\n')
    source = args.source.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    command = [pdflatex, '-interaction=nonstopmode', '-halt-on-error',
               '-no-shell-escape', f'-output-directory={output}', str(source)]
    for _ in range(2):
        subprocess.run(command, check=True, cwd=source.parent)


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
