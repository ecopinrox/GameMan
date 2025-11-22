########################################################################
####################### Makefile Template ##############################
########################################################################

# Compiler settings - Can be customized.
CC = gcc
CXXFLAGS = -std=c11 -Wall -I ./include/
LDFLAGS = -L ./lib/ -lopengl32 -lraylib -lgdi32 -lwinmm

# Makefile settings - Can be customized.
APPNAME = emulator
EXT = .c
SRCDIR = src
OBJDIR = obj

############## "Do not change anything from here downwards!": 🤓 #############
SRC = $(wildcard $(SRCDIR)/*$(EXT))				#src/*.c
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o) 	#obj/*.o
DEP = $(OBJ:$(OBJDIR)/%.o=$(OBJDIR)/%.d)		#obj/*.d (IDEALLY)
# UNIX-based OS variables & settings
RM = rm
DELOBJ = $(OBJ)
# Windows OS variables & settings
DEL = del
EXE = .exe
WDELOBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)\\%.o)
WDELDEP = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)\\%.d)

########################################################################
####################### Targets beginning here #########################
########################################################################

all: $(APPNAME)

# Builds the app
$(APPNAME): $(OBJ)
	$(CC) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Creates the dependecy rules
%.d: $(SRCDIR)/%$(EXT)
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:%.d=$(OBJDIR)/%.o) >$@

# Includes all .h files
-include $(DEP)

# Building rule for .o files and its .c/.cpp in combination with all .h
$(OBJDIR)/%.o: $(SRCDIR)/%$(EXT)
	$(CC) $(CXXFLAGS) -o $@ -c $<

################### Cleaning rules for Unix-based OS ###################
# Cleans complete project
.PHONY: clean
clean:
	$(RM) $(DELOBJ) $(DEP) $(APPNAME)

# Cleans only all files with the extension .d
.PHONY: cleandep
cleandep:
	$(RM) $(DEP)

#################### Cleaning rules for Windows OS #####################
# Cleans complete project except files with the extension .d (which aren't being created for some reason anyway)
.PHONY: cleanw
cleanw:
	$(DEL) $(WDELOBJ) $(APPNAME) $(EXE)

# Cleans only all files with the extension .d
.PHONY: cleandepw
cleandepw:
	$(DEL) $(DEP)