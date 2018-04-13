#
# OMNeT++/OMNEST Makefile for WandstemMac
#
# This file was generated with the command:
#  opp_makemake -f --deep -I.
#

# Name of target to be created (-o option)
TARGET = WandstemMac$(D)$(EXE_SUFFIX)
TARGET_DIR = .

# User interface (uncomment one) (-u option)
USERIF_LIBS = $(ALL_ENV_LIBS) # that is, $(TKENV_LIBS) $(QTENV_LIBS) $(CMDENV_LIBS)
#USERIF_LIBS = $(CMDENV_LIBS)
#USERIF_LIBS = $(TKENV_LIBS)
#USERIF_LIBS = $(QTENV_LIBS)

# C++ include paths (with -I)
INCLUDE_PATH = -I.

# Additional object and library files to link with
EXTRA_OBJS =

# Additional libraries (-L, -l options)
LIBS =

# Output directory
PROJECT_OUTPUT_DIR = ../out
PROJECTRELATIVE_PATH = src
O = $(PROJECT_OUTPUT_DIR)/$(CONFIGNAME)/$(PROJECTRELATIVE_PATH)

# Object files for local .cpp, .msg and .sm files
OBJS = \
    $O/miosix_utils_sim.o \
    $O/MiosixInterface.o \
    $O/MiosixStaticInterface.o \
    $O/Node.o \
    $O/NodeBase.o \
    $O/RootNode.o \
    $O/interfaces-impl/power_manager.o \
    $O/interfaces-impl/transceiver.o \
    $O/interfaces-impl/virtual_clock.o \
    $O/kernel/timeconversion.o \
    $O/network_module/debug_settings.o \
    $O/network_module/dynamic_mac_context.o \
    $O/network_module/dynamic_medium_access_controller.o \
    $O/network_module/mac_context.o \
    $O/network_module/mac_phase.o \
    $O/network_module/master_mac_context.o \
    $O/network_module/master_medium_access_controller.o \
    $O/network_module/medium_access_controller.o \
    $O/network_module/network_configuration.o \
    $O/network_module/data/dataphase.o \
    $O/network_module/schedule/dynamic_schedule_downlink.o \
    $O/network_module/schedule/dynamic_schedule_information.o \
    $O/network_module/schedule/master_schedule_downlink.o \
    $O/network_module/schedule/master_schedule_information.o \
    $O/network_module/schedule/schedule_context.o \
    $O/network_module/schedule/schedule_downlink.o \
    $O/network_module/schedule/schedule_information.o \
    $O/network_module/timesync/dynamic_timesync_downlink.o \
    $O/network_module/timesync/master_timesync_downlink.o \
    $O/network_module/timesync/sync_status.o \
    $O/network_module/timesync/timesync_downlink.o \
    $O/network_module/timesync/controller/flopsync1.o \
    $O/network_module/timesync/controller/flopsync2.o \
    $O/network_module/timesync/controller/synchronizer.o \
    $O/network_module/timesync/roundtrip/asking_roundtrip.o \
    $O/network_module/timesync/roundtrip/listening_roundtrip.o \
    $O/network_module/timesync/roundtrip/roundtrip_subphase.o \
    $O/network_module/uplink/dynamic_uplink_phase.o \
    $O/network_module/uplink/master_uplink_phase.o \
    $O/network_module/uplink/uplink_message.o \
    $O/network_module/uplink/uplink_phase.o \
    $O/network_module/uplink/stream_management/stream_management_context.o \
    $O/network_module/uplink/stream_management/stream_management_element.o \
    $O/network_module/uplink/topology/mesh_topology_context.o \
    $O/network_module/uplink/topology/neighbor.o \
    $O/network_module/uplink/topology/runtime_bitset.o \
    $O/network_module/uplink/topology/topology_context.o \
    $O/network_module/uplink/topology/topology_map.o \
    $O/network_module/uplink/topology/topology_message.o \
    $O/network_module/uplink/topology/tree_topology_context.o

# Message files
MSGFILES =

# SM files
SMFILES =

#------------------------------------------------------------------------------

# Pull in OMNeT++ configuration (Makefile.inc)

ifneq ("$(OMNETPP_CONFIGFILE)","")
CONFIGFILE = $(OMNETPP_CONFIGFILE)
else
ifneq ("$(OMNETPP_ROOT)","")
CONFIGFILE = $(OMNETPP_ROOT)/Makefile.inc
else
CONFIGFILE = $(shell opp_configfilepath)
endif
endif

ifeq ("$(wildcard $(CONFIGFILE))","")
$(error Config file '$(CONFIGFILE)' does not exist -- add the OMNeT++ bin directory to the path so that opp_configfilepath can be found, or set the OMNETPP_CONFIGFILE variable to point to Makefile.inc)
endif

include $(CONFIGFILE)

# Simulation kernel and user interface libraries
OMNETPP_LIBS = $(OPPMAIN_LIB) $(USERIF_LIBS) $(KERNEL_LIBS) $(SYS_LIBS)

COPTS = $(CFLAGS) $(IMPORT_DEFINES)  $(INCLUDE_PATH) -I$(OMNETPP_INCL_DIR)
MSGCOPTS = $(INCLUDE_PATH)
SMCOPTS =

# we want to recompile everything if COPTS changes,
# so we store COPTS into $COPTS_FILE and have object
# files depend on it (except when "make depend" was called)
COPTS_FILE = $O/.last-copts
ifneq ("$(COPTS)","$(shell cat $(COPTS_FILE) 2>/dev/null || echo '')")
$(shell $(MKPATH) "$O" && echo "$(COPTS)" >$(COPTS_FILE))
endif

#------------------------------------------------------------------------------
# User-supplied makefile fragment(s)
# >>>
# <<<
#------------------------------------------------------------------------------

# Main target
all: $(TARGET_DIR)/$(TARGET)

$(TARGET_DIR)/% :: $O/%
	@mkdir -p $(TARGET_DIR)
	$(Q)$(LN) $< $@
ifeq ($(TOOLCHAIN_NAME),clangc2)
	$(Q)-$(LN) $(<:%.dll=%.lib) $(@:%.dll=%.lib)
endif

$O/$(TARGET): $(OBJS)  $(wildcard $(EXTRA_OBJS)) Makefile $(CONFIGFILE)
	@$(MKPATH) $O
	@echo Creating executable: $@
	$(Q)$(CXX) $(LDFLAGS) -o $O/$(TARGET) $(OBJS) $(EXTRA_OBJS) $(AS_NEEDED_OFF) $(WHOLE_ARCHIVE_ON) $(LIBS) $(WHOLE_ARCHIVE_OFF) $(OMNETPP_LIBS)

.PHONY: all clean cleanall depend msgheaders smheaders

.SUFFIXES: .cpp

$O/%.o: %.cpp $(COPTS_FILE) | msgheaders smheaders
	@$(MKPATH) $(dir $@)
	$(qecho) "$<"
	$(Q)$(CXX) -c $(CXXFLAGS) $(COPTS) -o $@ $<

%_m.cpp %_m.h: %.msg
	$(qecho) MSGC: $<
	$(Q)$(MSGC) -s _m.cpp $(MSGCOPTS) $?

%_sm.cpp %_sm.h: %.sm
	$(qecho) SMC: $<
	$(Q)$(SMC) -c++ -suffix cpp $(SMCOPTS) $?

msgheaders: $(MSGFILES:.msg=_m.h)

smheaders: $(SMFILES:.sm=_sm.h)

clean:
	$(qecho) Cleaning $(TARGET)
	$(Q)-rm -rf $O
	$(Q)-rm -f $(TARGET_DIR)/$(TARGET)
	$(Q)-rm -f $(TARGET_DIR)/$(TARGET:%.dll=%.lib)
	$(Q)-rm -f $(call opp_rwildcard, . , *_m.cpp *_m.h *_sm.cpp *_sm.h)

cleanall:
	$(Q)$(MAKE) -s clean MODE=release
	$(Q)$(MAKE) -s clean MODE=debug
	$(Q)-rm -rf $(PROJECT_OUTPUT_DIR)

# include all dependencies
-include $(OBJS:%.o=%.d)
