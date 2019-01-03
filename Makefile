CC=clang++-6.0
APP=satnow-cpp
CFLAGS=-g3 -O0 --std=c++14
LIBS=ex/sgp4/build/libsgp4/libsgp4.a
INCS=-Iex/sgp4/libsgp4/

$(APP): main.cc
	$(CC) $^ -o $@ $(CFLAGS) $(INCS) $(LIBS)

clean:
	$(RM) $(APP)
