CC = g++-4.9
LDFLAGS = -std=c++11 -O2 -I../../flotilla-daemon/lib/websocketpp/ -I../../flotilla-daemon/lib/ -lserialport -lboost_system -lpthread
CFLAGS = -c -std=c++11 -O2 -I../../flotilla-daemon/lib/websocketpp/

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
