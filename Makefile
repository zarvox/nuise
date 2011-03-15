OPTS=-Werror
OBJS=main.o nuise.o loader.o
INCLUDE=-I/usr/include/libusb-1.0
LIBS=-lusb-1.0
DATFILES=cancelled.dat channel1.dat channel2.dat channel3.dat channel4.dat
WAVFILES=cancelled.wav channel1.wav channel2.wav channel3.wav channel4.wav

nuise: $(OBJS)
	gcc $(OPTS) -o nuise $(OBJS) $(INCLUDE) $(LIBS)

main.o: main.c
	gcc $(OPTS) -o main.o -c main.c $(INCLUDE)

nuise.o: nuise.c
	gcc $(OPTS) -o nuise.o -c nuise.c $(INCLUDE)

loader.o: loader.c
	gcc $(OPTS) -o loader.o -c loader.c $(INCLUDE)

wav: $(DATFILES)
	python convert.py

play: $(WAVFILES)
	mplayer *.wav

clean:
	rm -f nuise $(DATFILES) $(WAVFILES) $(OBJS)
