OPTS=-Werror
OBJS=main.o nuise.o loader.o
INCLUDE=-I/usr/include/libusb-1.0
LIBS=-lusb-1.0
DATFILES=cancelled.dat channel1.dat channel2.dat channel3.dat channel4.dat
WAVFILES=cancelled.wav channel1.wav channel2.wav channel3.wav channel4.wav
UPDATEFILE="SystemUpdate.zip"

nuise: $(OBJS) audios.bin
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

audios.bin:
	echo "Downloading Xbox360 system update package to extract audio firmware..."
	wget -O$(UPDATEFILE) "http://www.xbox.com/system-update-usb"
	echo "update package download complete.  Unzipping..."
	unzip $(UPDATEFILE)
	echo "Downloading Xbox360 file extracter..."
	wget -Oextract360.py ftp://rene-ladan.nl/pub/distfiles/extract360.py
	echo "Extracting audio firmware from update..."
	python extract360.py '$$systemupdate/FFFE07DF00000001'
	mv FFFE07DF00000001.dir/audios.bin ./
	echo "audios.bin extracted successfully, cleaning up."
	rm -rf FFFE07DF00000001.dir '$$systemupdate'
	rm FFFE07DF00000001.txt
	rm extract360.py
	rm $(UPDATEFILE)

clean:
	rm -f nuise $(DATFILES) $(WAVFILES) $(OBJS)
