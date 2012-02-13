###############################################################################
# Author:  Thomas Volkert
# Since:   2012-02-10
###############################################################################

TOP_DIR=$(CURDIR)/..

HOMER_GUI=Homer
HOMER_BASE=HomerBase
HOMER_MONITOR=HomerMonitor
HOMER_GAPI=HomerGAPI
HOMER_MULTIMEDIA=HomerMultimedia
HOMER_CONFERENCE=HomerConference
HOMER_SOUNDOUTPUT=HomerSoundOutput

###############################################################################
# catch all targets
default_target: debug
.PHONY : debug
%:
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_BASE)),)
	@cd $(TOP_DIR)/$(HOMER_BASE) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_MONITOR)),)
	@cd $(TOP_DIR)/$(HOMER_MONITOR) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_GAPI)),)
	@cd $(TOP_DIR)/$(HOMER_GAPI) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_MULTIMEDIA)),)
	@cd $(TOP_DIR)/$(HOMER_MULTIMEDIA) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_CONFERENCE)),)
	@cd $(TOP_DIR)/$(HOMER_CONFERENCE) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_SOUNDOUTPUT)),)
	@cd $(TOP_DIR)/$(HOMER_SOUNDOUTPUT) && $(MAKE) -s $*
endif
ifneq ($(wildcard $(TOP_DIR)/$(HOMER_GUI)),)
	@cd $(TOP_DIR)/$(HOMER_GUI) && $(MAKE) -s $*
endif
