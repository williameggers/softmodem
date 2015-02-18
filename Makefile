all: 
	(cd src; $(MAKE) all)
withgui:
	(cd src; $(MAKE) withgui)
clean:
	(cd src; $(MAKE) clean)
man:
	mandoc sm.8 > sm.cat8
	mandoc -Txhtml sm.8 |sed -e "0!N;s/<\/head>\n<body>/<\/head><body>/" > sm.html
