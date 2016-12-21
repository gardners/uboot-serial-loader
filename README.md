# uboot-serial-loader
Tool for flashing on systems with serial as the only available channel, and where uboot lacks the load[b/s/y] commands

This tool was created due to problems we faced with prototyping the 2nd generation Mesh Extender.  The prototype PCBs
had non-functional ethernet ports, so when we accidentaly loaded a broken OpenWRT image, we couldn't tftpboot a new one.
To make matters worse, the uboot on the Domino Core boards lacks the loady and related commands that allow efficient
loading of data via the serial port.

The principle of this program is simple: Write to batches of memory addresses at once (to avoid USB serial adapter
latency problems), and then read back the data to verify that it has been written correctly.

Since we are communicating with das U-Boot, we call our utility "Gurtude" after the submarine telephone of the same name.

... oh, it looks like someone else has made such a thing in python already:

https://galax.is/files/802/write-to-uboot.py

We will test this.  A niggling concern is whether that one will work fast on usb serial adapters, as they typically
add a LOT of latency to serial round-trips.  

Anyway, our utility now largely works, writing 1KB blocks and verifying them.  A nice bonus is that it can resume
relatively quickly, because the verification proces is much faster than writing.  So if the program is stopped for
some reason, you can run it again, and it will make use of whatever is already loaded into memory.  

It always loads the image at 0x1000000. This should be made configurable.
