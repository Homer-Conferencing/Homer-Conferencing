###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-10
###############################################################################

TOP_DIR=$(CURDIR)
-include ../HomerBuild/MakeCore

HOMER_BASE=HomerBase
HOMER_CONFERENCE=HomerConference
HOMER_NAPI=HomerNAPI
HOMER_GUI=Homer
HOMER_MONITOR=HomerMonitor
HOMER_MULTIMEDIA=HomerMultimedia
HOMER_NETWORK_SIMULATOR=../HomerNetworkSimulator/HomerNetworkSimulator
HOMER_SOUNDOUTPUT=HomerSoundOutput

###############################################################################
distclean: cleaner
ifneq ($(wildcard $(CURDIR)/../$(HOMER_BASE)),)
	@cd $(CURDIR)/../$(HOMER_BASE) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_MONITOR)),)
	@cd $(CURDIR)/../$(HOMER_MONITOR) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_NAPI)),)
	@cd $(CURDIR)/../$(HOMER_NAPI) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_MULTIMEDIA)),)
	@cd $(CURDIR)/../$(HOMER_MULTIMEDIA) && $(MAKE) -s cleaner
endif
ifneq ($(wildcard $(CURDIR)/../$(HOMER_NETWORK_SIMULATOR)),)
	@cd $(CURDIR)/../$(HOMER_NETWORK_SIMULATOR) && $(MAKE) -s cleaner
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

