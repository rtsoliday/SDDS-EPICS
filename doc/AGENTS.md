# Documentation Style Guide

This directory contains LaTeX source files describing each SDDS EPICS command line tool. Follow these conventions when adding or updating documentation.

## File naming
- One program per `.tex` file.
- File names use the command name, e.g. `sddsplot.tex`.

## Structure
Each program is documented using the `sddsprog` environment defined in `sddstoolkit.sty`:

```
\begin{sddsprog}{<toolName>}
\item \textbf{description:} one or more paragraphs describing the program.
\item \textbf{examples:}
\begin{verbatim}
command examples...
\end{verbatim}
\item \textbf{synopsis:}
\begin{verbatim}
usage: <toolName> [options]
\end{verbatim}
\item \textbf{switches:} optional `itemize` block describing command-line switches.
\item \textbf{files:} optional block describing input or output files.
\item \textbf{see also:} optional references using `\progref{otherTool}`.
\item \textbf{author:} Name, ANL/APS.
\end{sddsprog}
```

Use the `verbatim` environment for both examples and synopsis sections to ensure consistent formatting.

## Formatting
- Indent using two spaces; do not use tabs.
- Use `\verb|command|` or `{\tt text}` for inline code.
- Keep LaTeX commands and environments consistent with the existing files.

## Building
The build requires LaTeX and DVIPS from TeX Live, Ghostscript, TeX4ht, and
ImageMagick. See `README.md` in this directory for package suggestions and the
complete output list. Check the active environment before building:

```
make check-deps
```

Use the provided `Makefile` to generate PostScript, PDF, and HTML output:

```
make
```
