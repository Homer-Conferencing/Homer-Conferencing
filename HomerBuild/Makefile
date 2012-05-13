###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-10
###############################################################################

TOP_DIR=$(CURDIR)
-include ../HomerBuild/MakeCore

HOMER_BASE=HomerBase
HOMER_CONFERENCE=HomerConference
HOMER_GAPI=HomerGAPI
HOMER_GUI=Homer
HOMER_MONITOR=HomerMonitor
HOMER_MULTIMEDIA=HomerMultimedia
HOMER_SOUNDOUTPUT=HomerSoundOutput

###############################################################################
distclean: cleaner
ifneq ($(wildcard $(CURDIR)/../$(HOMER_BASE)),)
	@cd $(CURDIR)/../$(HOMER_BASE) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_MONITOR)),)
	@cd $(CURDIR)/../$(HOMER_MONITOR) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_GAPI)),)
	@cd $(CURDIR)/../$(HOMER_GAPI) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_MULTIMEDIA)),)
	@cd $(CURDIR)/../$(HOMER_MULTIMEDIA) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_CONFERENCE)),)
	@cd $(CURDIR)/../$(HOMER_CONFERENCE) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_SOUNDOUTPUT)),)
	@cd $(CURDIR)/../$(HOMER_SOUNDOUTPUT) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_GUI)),)
	@cd $(CURDIR)/../$(HOMER_GUI) && $(MAKE) -s cleaner
endif

