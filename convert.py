#!/usr/bin/env python

basenames = ["cancelled", "channel1", "channel2", "channel3", "channel4" ]
import wave
first = True
for b in basenames:
	infname = b+".dat"
	outfname = b+".wav"
	wf = wave.open(outfname, "wb")
	wf.setnchannels(1)
	if first:
		wf.setsampwidth(2)
		first = False
	else:
		wf.setsampwidth(4)
	wf.setframerate(16000)
	wf.writeframes(open(infname).read())
	wf.close()
	print "Made", outfname, "from", infname
