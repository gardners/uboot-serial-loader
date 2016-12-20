# uboot-serial-loader
Tool for flashing on systems with serial as the only available channel, and where uboot lacks the load[b/s/y] commands

This tool was created due to problems we faced with prototyping the 2nd generation Mesh Extender.  The prototype PCBs
had non-functional ethernet ports, so when we accidentaly loaded a broken OpenWRT image, we couldn't tftpboot a new one.
To make matters worse, the uboot on the Domino Core boards lacks the loady and related commands that allow efficient
loading of data via the serial port.

The principle of this program is simple: Write to batches of memory addresses at once (to avoid USB serial adapter
latency problems), and then read back the data to verify that it has been written correctly.

Since we are communicating with das U-Boot, we call our utility "Gurtude" after the submarine telephone of the same name.
