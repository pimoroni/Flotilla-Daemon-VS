CC = g++
DEPS = ../../lib/
LDFLAGS = -O2 -std=c++11 -L. -lserialport -lboost_system -lboost_date_time -lboost_filesystem -lboost_program_options -lpthread
CFLAGS = -O2 -c -std=c++11 -I${DEPS}websocketpp/ -Wall

SOURCES=Discovery.cpp Timestamp.cpp Flotilla.cpp Flotilla_Dock.cpp Flotilla_Module.cpp Flotilla_Client.cpp Main.cpp
BUILDDIR=Debug
OBJECTS=$(addprefix $(BUILDDIR)/,$(notdir $(SOURCES:.cpp=.o)))
EXECUTABLE=flotilla

all: $(BUILDDIR) $(SOURCES) $(EXECUTABLE)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(BUILDDIR)/$@

$(BUILDDIR)/%.o: %.cpp
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -r $(BUILDDIR)

.PHONY: clean
