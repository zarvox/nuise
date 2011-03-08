OPTS=-Werror
INCLUDE=-I/usr/include/libusb-1.0
LIBS=-lusb-1.0
DATFILES=cancelled.dat channel1.dat channel2.dat channel3.dat channel4.dat
WAVFILES=cancelled.wav channel1.wav channel2.wav channel3.wav channel4.wav

nuise: nuise.c
	gcc $(OPTS) -o nuise nuise.c $(INCLUDE) $(LIBS)

wav: $(DATFILES)
	python convert.py

play: $(WAVFILES)
	mplayer *.wav

clean:
	rm -f nuise $(DATFILES) $(WAVFILES)
