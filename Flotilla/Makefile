CC = g++
DEPS = ../../flotilla-daemon/lib/
#DEPS = ../../flotilla-dependencies/
LDFLAGS = -O2 -std=c++11 -I${DEPS}websocketpp/ -I${DEPS} -lserialport -lboost_system -lboost_program_options -lpthread
CFLAGS = -O2 -c -std=c++11 -I${DEPS}websocketpp/

SOURCES=Flotilla.cpp Flotilla_Dock.cpp Flotilla_Module.cpp Flotilla_Client.cpp Main.cpp
BUILDDIR=Debug
OBJECTS=$(addprefix $(BUILDDIR)/,$(notdir $(SOURCES:.cpp=.o)))
EXECUTABLE=flotilla

all: $(BUILDDIR) $(SOURCES) $(EXECUTABLE)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $(BUILDDIR)/$@

$(BUILDDIR)/%.o: %.cpp
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -r $(BUILDDIR)

.PHONY: clean
