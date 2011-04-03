#!/usr/bin/env python

from urllib2 import Request, urlopen, URLError
import StringIO
import zipfile

def getFileOrURL(filename, url):
	# Check if a file named filename exists on disk.
	# If so, return its contents.  If not, download it, save it, and return its contents.
	try:
		f = open(filename)
		print "Found", filename, "cached on disk, using local copy"
		retval = f.read()
		return retval
	except IOError, e:
		pass
	print "Downloading", filename, "from", url
	req = Request(url)
	try:
		response = urlopen(req)
	except URLError, e:
		if hasattr(e, 'reason'):
			print "Failed to reach download server.  Reason:", e.reason
		elif hasattr(e, 'code'):
			print "The server couldn't fulfill the request.  Error code:", e.code
	print "Reading response..."
	retval = response.read()
	# Save downloaded file to disk
	f = open(filename, "wb")
	f.write(retval)
	f.close()
	print "done, saved to", filename
	return retval

def extractPirsFromZip(systemupdate):
	print "Extracting $systemupdate/FFFE07DF00000001 from system update file..."
	updatefile = StringIO.StringIO(systemupdate)
	z = zipfile.ZipFile(updatefile)
	#print z.namelist()
	pirs = z.open("$systemupdate/FFFE07DF00000001").read()
	print "done."
	return pirs

if __name__ == "__main__":

	fw = getFileOrURL("SystemUpdate.zip", "http://www.xbox.com/system-update-usb")
	extract360 = getFileOrURL("extract360.py", "ftp://rene-ladan.nl/pub/distfiles/extract360.py")
	pirs = extractPirsFromZip(fw)
	exec extract360

	sio = StringIO.StringIO(pirs)
	basename = "FFFE07DF00000001"
	sio.name = basename
	pwd = os.getcwd()
	handle_live_pirs(sio, len(pirs)-4)
	os.chdir(pwd)
	print "Moving audios.bin to current folder"
	os.rename(os.path.join(basename + ".dir", "audios.bin"), "audios.bin")
	print "Cleaning up"
	os.unlink(basename + ".txt")
	for root, dirs, files in os.walk(basename + ".dir"):
		for name in files:
			os.remove(os.path.join(root, name))
		for name in dirs:
			os.rmdir(os.path.join(root, name))
		os.rmdir(root)
	os.unlink("SystemUpdate.zip")
	os.unlink("extract360.py")
	print "Done!"
