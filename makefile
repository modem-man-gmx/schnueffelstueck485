.phony: clean all run

PROGNAME:=Schnueffelstueck485
CXX = g++
CXXFLAGS = -ggdb -Wall

libs=modbus
HEADERS=st485.h makefile
OBJECTS=st485_main.o

#$(OBJECTS): $(HEADERS)

%.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS)

$(PROGNAME): $(OBJECTS)
	$(CXX) -o $(PROGNAME) $^ -l $(libs)
	@echo 'OKAY'

all: $(PROGNAME)

clean:
	rm $(PROGNAME) $(OBJECTS)

run:
	./$(PROGNAME) --device /dev/ttyUSB1 # --verbose --debug
