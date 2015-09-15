
CC=gcc
CPP=g++
LD=g++
#
# Build release or debug version
#
#Compiler flags
ifdef RELEASE
DLAGS = -O
else
DFLAGS = -O2 -g -mhard-float
endif

CFLAGS = $(DFLAGS) -W -Wall -Wextra -fPIC -O3 -std=c++11

#Linker flags
LDFLAGS =

SOFLAGS =-W -WALL -pipe

#commands
RM = /bin/rm -f
MKDIR=/bin/mkdir -p
CPDIR=/bin/cp -r
CPFILE=/bin/cp
AR=/usr/bin/ar

OBJS = \
	src/.obj/LinuxSocket.o \
    src/.obj/server.o

all: \
     compileStats \
     src/.obj \
     bin \
     $(OBJS) \
     bin/server

clean:
	$(RM) -r src/.obj
	$(RM) -r bin

clean_all: clean
	$(RM) -r lib

compileStats:
	echo "extern \"C\" {" > src/compileStats.h;
	git rev-parse HEAD | awk '{print "  const char *compiled_git_sha = \"" $$0"\";"}' >> src/compileStats.h;
	(git symbolic-ref HEAD || echo "no branch") | sed 's/refs\/heads\///g' | awk '{print "  const char *compiled_git_branch = \"" $$0"\";"}'	 >> src/compileStats.h;
	echo "  const char *compiled_host = \"$$HOSTNAME\";" >> src/compileStats.h;
	echo "  const char *compiled_user = \"$$USER\";" >> src/compileStats.h;
	date +"%a %b %d %T %Z %Y" |  awk '{print "  const char *compiled_date = \"" $$0"\";"}' >> src/compileStats.h;
	uname -a |  awk '{print "  const char *compiled_note = \"" $$0"\";"}' >> src/compileStats.h;
	echo "}" >> src/compileStats.h

# compile imgAPO objs 
src/.obj/LinuxSocket.o: src/LinuxSocket.cpp
	$(CPP) $(CFLAGS) -c $< -o $@

src/.obj/server.o: src/server.cpp
	$(CPP) $(CFLAGS)  -c $< -o $@

bin/server: $(OBJS)
	$(LD) $(LDFLAGS) -rdynamic -o $@ $(OBJS) $(LIBS)

src/.obj:
	$(MKDIR) src/.obj
bin:
	$(MKDIR) bin
lib:
	$(MKDIR) lib