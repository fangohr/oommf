TARGET = ../../darwin/oxs
SRCS = $(shell ls *.cc)
HEADS = $(shell ls *.h)
COMMONSRCS = yy_mel_util.cc
COMMONHEADS = yy_mel_util.h
OBJSDIR = ../../darwin
OBJS = $(addprefix $(OBJSDIR)/,$(subst .cc,.o,$(SRCS)))

LOCALDIR = ../../local
OOMMFDIR = ../../../..

TESTMIF = testdata/Hyz_02_run.mif
TESTMIF_STEP = testdata/Hyz_02_run_step.mif
TESTMIF_STEP_FILELIST = testdata/Hyz_02_run_step_filelist.mif
TESTMIF_STRAIN_STEP = testdata/Hyz_02_run_strain_step.mif
TESTMIF_STRAIN_STEP_FILELIST = testdata/Hyz_02_run_strain_step_filelist.mif

TESTMIF_FIXED = testdata/Hyz_02_run_fixed.mif
TESTMIF_FIXED_STAGEMEL = testdata/Hyz_02_run_fixed_stagemel.mif
TESTMIF_FIXED_STRAIN = testdata/Hyz_02_run_fixed_strain.mif

TESTMIF_TRANSFORM = testdata/Hyz_02_run_transform.mif
TESTMIF_TRANSFORM_STRAIN = testdata/Hyz_02_run_transform_strain.mif

.SUFFIXES: .cc .h .o

RM = rm
CP = cp
OOMMF = tclsh $(OOMMFDIR)/oommf.tcl

all: $(OBJS)
	$(OOMMF) pimake -cwd $(OOMMFDIR)

$(OBJSDIR)/%.o: %.cc %.h $(COMMONSRCS) $(COMMONHEADS)
	$(CP) $*.cc $(LOCALDIR)
	$(CP) $*.h $(LOCALDIR)

clean:
	$(RM) -f $(OBJS) $(TARGET) *~ \#*\#

test:
	$(OOMMF) oxsii $(TESTMIF)

test_step:
	$(OOMMF) oxsii $(TESTMIF_STEP)

test_step_filelist:
	$(OOMMF) oxsii $(TESTMIF_STEP_FILELIST)

test_strain_step:
	$(OOMMF) oxsii $(TESTMIF_STRAIN_STEP)

test_strain_step_filelist:
	$(OOMMF) oxsii $(TESTMIF_STRAIN_STEP_FILELIST)

test_fixed:
	$(OOMMF) oxsii $(TESTMIF_FIXED)

test_fixed_stagemel:
	$(OOMMF) oxsii $(TESTMIF_FIXED_STAGEMEL)

test_fixed_strain:
	$(OOMMF) oxsii $(TESTMIF_FIXED_STRAIN)

test_transform:
	$(OOMMF) oxsii $(TESTMIF_TRANSFORM)

test_transform_strain:
	$(OOMMF) oxsii $(TESTMIF_TRANSFORM_STRAIN)

# Dependency rule
#-include Makefile.depend
