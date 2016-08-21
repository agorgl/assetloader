PRJTYPE = StaticLib
ifeq ($(OS), Windows_NT)
	DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE
endif

DEMODIR = demo
demo: build
	@$(MAKE) -C $(strip $(DEMODIR)) -f $(MKLOC) clean
	@$(MAKE) -C $(strip $(DEMODIR)) -f $(MKLOC) run
