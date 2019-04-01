# Mac OS X notes for PkTriggerCord

## Building & Debugging

An Xcode project file is included. To run it inside Xcode's debugging environment, you need to make some preparations:

1. Edit the scheme (Menu: Project / Scheme / Edit Scheme).
2. In the Run settings, see the Arguments Passed On Launch:
	1. 	Edit the `--device=diskX`, where X is the disk number of the connected Pentax camera (when connecting the camera, its internal (primary) memory card should appear as a volume - you can figure out its disk number with Disk Utility's Info window).
	2. You may want to update the other arguments as well.
3. In Terminal enter: `sudo chmod 666 /dev/diskX` (with X changed to the disk number). This will allow you to access the disk without a `sudo` command.
4. If you then still get "Permission denied" errors when you Run, you may have to disable [SIP](https://en.wikipedia.org/wiki/System_Integrity_Protection).

## Current state of Mac version

Despite all efforts, this tool does not work on recent macOS versions, yet. Main problem seems to be missing SCSI command (pass-through) support. See [here](https://github.com/asalamon74/pktriggercord/issues/38) for some more information.
