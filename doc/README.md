# SDDS-EPICS manuals

This directory contains the LaTeX sources for three manuals:

- `EPICStoolkit.tex` — SDDS-compliant EPICS command-line applications.
- `APSRunControl.tex` — the APS run-control library and protocol.
- `APSlogDaemon.tex` — the APS distributed logging service.

Most EPICS Toolkit commands have one program-specific `.tex` file that uses
the `sddsprog` environment from `sddstoolkit.sty`. See
[`AGENTS.md`](AGENTS.md) for the source-format and manual-page conventions.

## Build requirements

The build directly uses these commands:

- `latex` and `dvips` from TeX Live
- `ps2pdf13` from Ghostscript
- `htlatex` from TeX4ht
- `convert` from ImageMagick (for the APS Run Control diagram)

On Debian or Ubuntu, a suitable installation is:

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends \
  texlive-base texlive-latex-base texlive-latex-extra \
  texlive-fonts-recommended texlive-plain-generic \
  tex4ht ghostscript imagemagick
```

Package names differ on other operating systems. Confirm the commands needed
by the makefile with:

```sh
make check-deps
```

Some ImageMagick installations disable EPS input in their security policy. If
the `convert rc.eps rc.png` step is rejected, enable EPS conversion according
to the local ImageMagick policy or retain the checked-in `rc.png` generated
from `rc.eps`.

## Building

Run the documentation build from this directory:

```sh
make
```

The following outputs are generated:

| Manual | PostScript | PDF | HTML directory |
| --- | --- | --- | --- |
| EPICS Toolkit | `EPICStoolkit.ps` | `EPICStoolkit.pdf` | `EPICStoolkit/` |
| APS Run Control | `APSRunControl.ps` | `APSRunControl.pdf` | `APSRunControl/` |
| APS Log Daemon | `APSlogDaemon.ps` | `APSlogDaemon.pdf` | `APSlogDaemon/` |

Remove all generated manual files with:

```sh
make clean
```

The application build and pytest workflow are documented in the repository
[`README.md`](../README.md).
