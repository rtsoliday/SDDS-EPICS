all:: check-deps EPICStoolkit.ps EPICStoolkit.pdf EPICStoolkit \
    APSRunControl.ps APSRunControl.pdf APSRunControl \
    APSlogDaemon.ps APSlogDaemon.pdf APSlogDaemon

.PHONY: check-deps
check-deps:
	@command -v latex >/dev/null || (echo "Error: latex command not found. Install TeX Live packages." >&2; exit 1)
	@command -v dvips >/dev/null || (echo "Error: dvips command not found. Install TeX Live binaries." >&2; exit 1)
	@command -v ps2pdf13 >/dev/null || (echo "Error: ps2pdf13 command not found. Install Ghostscript." >&2; exit 1)
	@command -v htlatex >/dev/null || (echo "Error: htlatex command not found. Install TeX4ht." >&2; exit 1)
	@command -v convert >/dev/null || (echo "Error: convert command not found. Install ImageMagick." >&2; exit 1)
	@echo "All documentation build dependencies are available."

EPICStoolkit.ps:
	rm -f EPICStoolkit.aux EPICStoolkit.dvi EPICStoolkit.log EPICStoolkit.toc EPICStoolkit.out
	latex EPICStoolkit
	latex EPICStoolkit
	dvips EPICStoolkit -o EPICStoolkit.ps

EPICStoolkit.pdf: EPICStoolkit.ps
	ps2pdf13 EPICStoolkit.ps EPICStoolkit.pdf

EPICStoolkit:
	mkdir -p EPICStoolkit
	htlatex EPICStoolkit "html,3" "" "-dEPICStoolkit/"
	rm -f *.html *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

APSRunControl.ps:
	rm -f APSRunControl.aux APSRunControl.dvi APSRunControl.log APSRunControl.toc APSRunControl.out
	latex APSRunControl
	latex APSRunControl
	dvips APSRunControl -o APSRunControl.ps

APSRunControl.pdf: APSRunControl.ps
	ps2pdf13 APSRunControl.ps APSRunControl.pdf

APSRunControl:
	mkdir -p APSRunControl
	convert rc.eps rc.png
	htlatex APSRunControl "html,3" "" "-dAPSRunControl/"
	rm -f *.html *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

APSlogDaemon.ps:
	rm -f APSlogDaemon.aux APSlogDaemon.dvi APSlogDaemon.log APSlogDaemon.toc APSlogDaemon.out
	latex APSlogDaemon
	latex APSlogDaemon
	dvips APSlogDaemon -o APSlogDaemon.ps

APSlogDaemon.pdf: APSlogDaemon.ps
	ps2pdf13 APSlogDaemon.ps APSlogDaemon.pdf

APSlogDaemon:
	mkdir -p APSlogDaemon
	htlatex APSlogDaemon "html,3" "" "-dAPSlogDaemon/"
	rm -f *.html *.css *.4ct *.tmp *.dvi *.idv *.4tc *.lg *.xref *.log *.aux

clean::
	rm -f EPICStoolkit.aux EPICStoolkit.dvi EPICStoolkit.log EPICStoolkit.toc EPICStoolkit.ps EPICStoolkit.pdf EPICStoolkit.out
	rm -rf EPICStoolkit
	rm -f APSRunControl.aux APSRunControl.dvi APSRunControl.log APSRunControl.toc APSRunControl.ps APSRunControl.pdf APSRunControl.out
	rm -rf APSRunControl
	rm -f APSlogDaemon.aux APSlogDaemon.dvi APSlogDaemon.log APSlogDaemon.toc APSlogDaemon.ps APSlogDaemon.pdf APSlogDaemon.out
	rm -rf APSlogDaemon
