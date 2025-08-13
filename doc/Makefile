all:: check-deps EPICStoolkit.ps EPICStoolkit.pdf EPICStoolkit \
	APSRunControl.ps APSRunControl.pdf APSRunControl \
	APSlogDaemon.ps APSlogDaemon.pdf APSlogDaemon

.PHONY: check-deps
check-deps:
	@command -v latex >/dev/null || (echo "Error: latex command not found. Install TeX Live packages." >&2; exit 1)
	@command -v latex2html >/dev/null || (echo "Error: latex2html command not found. Install latex2html package." >&2; exit 1)
	@command -v repstopdf >/dev/null || (echo "Error: repstopdf command not found. Install texlive-font-utils package." >&2; exit 1)
	@echo "All documentation build dependencies are available."

EPICStoolkit.ps:
	rm -f EPICStoolkit.aux EPICStoolkit.dvi EPICStoolkit.log EPICStoolkit.toc
	latex EPICStoolkit
	latex EPICStoolkit
	dvips EPICStoolkit -o EPICStoolkit.ps

EPICStoolkit.pdf: EPICStoolkit.ps
	ps2pdf13 EPICStoolkit.ps EPICStoolkit.pdf

EPICStoolkit:
	latex2html -local_icons EPICStoolkit

APSRunControl.ps:
	rm -f APSRunControl.aux APSRunControl.dvi APSRunControl.log APSRunControl.toc
	latex APSRunControl
	latex APSRunControl
	dvips APSRunControl -o APSRunControl.ps

APSRunControl.pdf: APSRunControl.ps
	ps2pdf13 APSRunControl.ps APSRunControl.pdf

APSRunControl:
	latex2html -local_icons APSRunControl

APSlogDaemon.ps:
	rm -f APSlogDaemon.aux APSlogDaemon.dvi APSlogDaemon.log APSlogDaemon.toc
	latex APSlogDaemon
	latex APSlogDaemon
	dvips APSlogDaemon -o APSlogDaemon.ps

APSlogDaemon.pdf: APSlogDaemon.ps
	ps2pdf13 APSlogDaemon.ps APSlogDaemon.pdf

APSlogDaemon:
	latex2html -local_icons APSlogDaemon

clean::
	rm -f EPICStoolkit.aux EPICStoolkit.dvi EPICStoolkit.log EPICStoolkit.toc EPICStoolkit.ps EPICStoolkit.pdf
	rm -rf EPICStoolkit
	rm -f APSRunControl.aux APSRunControl.dvi APSRunControl.log APSRunControl.toc APSRunControl.ps APSRunControl.pdf
	rm -rf APSRunControl
	rm -f APSlogDaemon.aux APSlogDaemon.dvi APSlogDaemon.log APSlogDaemon.toc APSlogDaemon.ps APSlogDaemon.pdf
	rm -rf APSlogDaemon
