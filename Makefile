EXE       = catch
EXE_OBJS  = $(EXE).o

CPPFLAGS  += -g -fpic -Wall -std=c++11

LDFLAGS += `pkg-config --cflags --libs libsystemd`

all: $(EXE)

%.o : %.c 
	$(CC) -c $< $(CFLAGS) $(INCLUDES) -o $@

%.o : %.C
	$(CXX) -c $< $(CPPFLAGS) -o $@


$(EXE): $(EXE_OBJS)
	$(CXX) $^ $(LDFLAGS) $(LIBS) -o $@

clean:
	rm -f $(CHECK) *.o *.so $(EXE)
