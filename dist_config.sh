PACKAGE=libt3config
SRCDIRS="src"
EXCLUDESRC="/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$"
GENSOURCES="`echo src/.objects/*_hide.h src/.objects/*.bytes` src/config_api.h src/config_errors.h src/config_shared.c"
VERSIONINFO="0:1:0"
