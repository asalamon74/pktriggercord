Some data gathered during OpenBSD development and testing.

Log files: 
root@linux# ./pktriggercord-cli -t 1/180 -o test --debug 2> log_linux.txt
root@openbsd# ./pktriggercord-cli -t 1/180 -o test --debug 2> log_openbsd.txt

convert test-linux-0000.dng -scale 1024x1024 test-linux.jpg
convert test-openbsd-0000.dng -scale 1024x1024 test-openbsd.jpg

test-openbsd.jpg DNG exported and scaled down on OpenBSD
test-linux.jpg  DNG exported and scaled down on Linux

exif_data.txt exiv2 *dng>exif_data.txt
