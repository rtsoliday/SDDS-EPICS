# Documentation Style Guide

This directory contains LaTeX source files describing each SDDS Epics command line tool. Follow these conventions when adding or updating documentation.

## File naming
- One program per `.tex` file.
- File names use the command name, e.g. `sddsplot.tex`.

## Structure
Each file begins with the new-page preamble and a subsection heading:

```
%\begin{latexonly}
\newpage
%\end{latexonly}
\subsection{<toolName>}
\label{<toolName>}

\begin{itemize}
```

Within the `itemize` block include, in order:
1. `\item {\bf description:}` – One or more paragraphs describing the program.
2. `\item {\bf examples:}` – Example commands wrapped in `\begin{flushleft}{\tt ... }\end{flushleft}` with line breaks using `\\`.
3. `\item {\bf synopsis:}` – Usage text in a `flushleft` block.
4. Optional items such as `\item {\bf files:}`, `\item {\bf switches:}`, and `\item {\bf see also:}`. Cross references use `\progref{otherTool}`.
5. `\item {\bf author:} Name, ANL/APS.`

End each document with:

```
\end{itemize}
```

## Formatting
- Indent using two spaces; do not use tabs.
- Use `\verb|command|` or `{\tt text}` for inline code.
- Keep LaTeX commands and environments consistent with the existing files.

## Building
Before running `make`, install the required LaTeX packages:

```bash
apt-get update
apt-get install -y --no-install-recommends \
  texlive-base texlive-latex-base texlive-latex-extra \
  texlive-fonts-recommended texlive-plain-generic \
  texlive-extra-utils tex4ht ghostscript
```

If an error related to `ca-certificates-java` appears, remove the OpenJDK packages:

```bash
apt-get remove --purge -y openjdk-*-jre-headless default-jre-headless default-jre
```

Use the provided `Makefile` to generate PostScript, PDF and HTML output:

```
make
```
