CXX=mpic++.mpich
CXXFLAGS=-g -Wall -Iinclude
CXXFLAGS_SO= -fPIC
LDLIBS=-L. -lmpi_monitor_utils

SRCDIR=src
EDIR=src/example

#OBJS=$(subst .cpp,.o,$(SRCDIR)/*.cpp)
#EOBJ=$(subst .cpp,.o,$(SRCDIR)/*.cpp)

_OBJS=mpi_cond.o mpi_mutex.o mpi_guard.o
OBJS=$(patsubst %,$(SRCDIR)/%,$(_OBJS))
_EOBJ=example.o
EOBJ=$(patsubst %,$(EDIR)/%,$(_EOBJ))

all: lib example

example: $(EOBJ)
	$(CXX) $(LDLIBS) $^ -o $@
	
libmpi_monitor_utils.so: $(OBJS)
	$(CXX) -shared $^ -o $@
	
$(OBJS): $(SRCDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS_SO) $(CXXFLAGS) $< -o $@

$(EOBJ): $(EDIR)/%.o: $(EDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@	

.PHONY: clean lib

clean:
	rm -f $(SRCDIR)/*.o
	rm -f $(EDIR)/*.o

lib: clean libmpi_monitor_utils.so
