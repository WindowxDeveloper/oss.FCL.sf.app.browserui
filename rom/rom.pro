TEMPLATE = subdirs
SUBDIRS = 

# Generate the rom iby file from the template.pkg files already generated by qmake
symbian {
	exists( $$PWD/../../../../brtools/platform/symbian/install/generate-rom-iby.bat ) {
		system($$PWD/../../../../brtools/platform/symbian/install/generate-rom-iby.bat)
	}
}