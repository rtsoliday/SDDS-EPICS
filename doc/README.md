# SDDS EPICS Documentation

The LaTeX sources in this directory document each SDDS EPICS command-line tool.  One program is described per `.tex` file using the `sddsprog` environment from `sddstoolkit.sty`.

## Build requirements

Install TeX Live packages and related tools:

```bash
apt-get update
apt-get install -y --no-install-recommends \
  texlive-base texlive-latex-base texlive-latex-extra \
  texlive-fonts-recommended texlive-plain-generic \
  texlive-extra-utils tex4ht ghostscript latex2html \
  texlive-font-utils
```

If an error referencing `ca-certificates-java` appears, remove the OpenJDK packages:

```bash
apt-get remove --purge -y openjdk-*-jre-headless default-jre-headless default-jre
```

See [AGENTS.md](AGENTS.md) for documentation style guidelines.

## Building

Use the provided `Makefile` to generate PostScript, PDF and HTML output:

```bash
make
```

PDF files (e.g. `EPICStoolkit.pdf`) are written to this directory and HTML output is placed in subdirectories such as `EPICStoolkit/`.

