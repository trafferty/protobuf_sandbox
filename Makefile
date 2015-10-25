
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

PROTOBUF_CFLAGS = $(shell pkg-config --cflags protobuf)
PROTOBUF_LIBS   = $(shell pkg-config --libs protobuf)

#commands
RM = /bin/rm -f
MKDIR=/bin/mkdir -p
CPDIR=/bin/cp -r
CPFILE=/bin/cp
AR=/usr/bin/ar
PROTOC=protoc

INCLUDES=\
	$(PROTOBUF_CFLAGS) \
	-I./src

UTIL_OBJS = \
    src/.obj/cJSON.o \
    src/.obj/CNT_JSON.o

OBJS = \
    src/.obj/payload.pb.o \
    src/.obj/LinuxSocket.o \
    src/.obj/SocketTransport.o \
    src/.obj/CommandProcessor.o

all: \
     compileStats \
     src/.obj \
     bin \
     $(UTIL_OBJS) \
     $(OBJS) \
     bin/client \
     bin/server

clean:
	$(RM) src/compileStats.h
	$(RM) -r src/.obj
	$(RM) -r src/*.pb.*
	$(RM) -r bin

compileStats:
	echo "extern \"C\" {" > src/compileStats.h;
	git rev-parse HEAD | awk '{print "  const char *compiled_git_sha = \"" $$0"\";"}' >> src/compileStats.h;
	(git symbolic-ref HEAD || echo "no branch") | sed 's/refs\/heads\///g' | awk '{print "  const char *compiled_git_branch = \"" $$0"\";"}'	 >> src/compileStats.h;
	echo "  const char *compiled_host = \"$$HOSTNAME\";" >> src/compileStats.h;
	echo "  const char *compiled_user = \"$$USER\";" >> src/compileStats.h;
	date +"%a %b %d %T %Z %Y" |  awk '{print "  const char *compiled_date = \"" $$0"\";"}' >> src/compileStats.h;
	uname -a |  awk '{print "  const char *compiled_note = \"" $$0"\";"}' >> src/compileStats.h;
	echo "}" >> src/compileStats.h

# compile proto file and obj
src/payload.pb.cc src/payload.pb.h: src/payload.proto
	$(PROTOC) --proto_path=$(dir $^) --cpp_out=$(dir $^) $^  
src/.obj/payload.pb.o: src/payload.pb.cc src/payload.pb.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES)

# compile util objs
src/.obj/cJSON.o: src/cJSON.c src/cJSON.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES)
src/.obj/CNT_JSON.o: src/CNT_JSON.c src/CNT_JSON.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES)

# compile Driver objs 
src/.obj/LinuxSocket.o: src/LinuxSocket.cpp src/LinuxSocket.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES)

src/.obj/SocketTransport.o: src/SocketTransport.cpp src/SocketTransport.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES) 

src/.obj/CommandProcessor.o: src/CommandProcessor.cpp src/CommandProcessor.h
	$(CPP) -c $< -o $@ $(CFLAGS) $(INCLUDES) 

# compile exe objs
src/.obj/server.o: src/server.cpp
	$(CPP) $(CFLAGS)  -c $< -o $@

src/.obj/client.o: src/client.cpp
	$(CPP) $(CFLAGS)  -c $< -o $@

# link bins
bin/client: src/.obj/client.o $(UTIL_OBJS) $(OBJS)
	$(LD) -o $@ $(LDFLAGS) src/.obj/client.o $(UTIL_OBJS) $(OBJS) $(PROTOBUF_LIBS)
	
bin/server: src/.obj/server.o $(UTIL_OBJS) $(OBJS)
	$(LD) -o $@ $(LDFLAGS) src/.obj/server.o $(UTIL_OBJS) $(OBJS) $(PROTOBUF_LIBS)

src/.obj:
	$(MKDIR) src/.obj
bin:
	$(MKDIR) bin
lib:
	$(MKDIR) lib
