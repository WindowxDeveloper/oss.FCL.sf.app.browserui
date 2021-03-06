TEMPLATE = subdirs
CONFIG += ordered

include (browserui.pri)
symbian : {
    contains(browser_addon, ninetwo) {
SUBDIRS += utilities
    }
}
SUBDIRS += internal/tests/perfTracing
SUBDIRS += qstmgesturelib
SUBDIRS += bedrockProvisioning
SUBDIRS += browsercore
SUBDIRS += ginebra2

symbian : {
    contains(browser_addon, no) {
        SUBDIRS += homescreen
    }
}
# rom MUST come last because it depends on *_template.pkg files generated by qmake from all the other apps
    SUBDIRS += rom

contains( what, tests ) {
    DEFINES += ENABLE_TESTS
    exists($$PWD/internal/tests/perfTracing/perfTracing.pro): SUBDIRS += internal/tests/perfTracing/perfTracing.pro
    exists($$PWD/internal/tests/mw/mw.pro): SUBDIRS += internal/tests/mw/mw.pro
    exists($$PWD/internal/tests/Bookmarks_Test/Bookmarks_Test.pro): SUBDIRS += internal/tests/mw/Bookmarks_Test/Bookmarks_Test.pro
}

symbian: { 

contains(browser_addon, no ) {
    BLD_INF_RULES.prj_exports += "$${LITERAL_HASH}include <platform_paths.hrh>" \
                                 "rom/browser.iby  CORE_APP_LAYER_IBY_EXPORT_PATH(browser.iby)" \
                                 "rom/browserresources.iby  LANGUAGE_APP_LAYER_IBY_EXPORT_PATH(browserresources.iby)" \
                                 "rom/NokiaBrowser_stub.sis  z:/system/install/NokiaBrowser_stub.sis
    }  
}
