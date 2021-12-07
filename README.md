Converts a SC68/SNDH files(http://fileformats.archiveteam.org/wiki/SNDH) to RAW PCM files using SC68.
Unlike `sc68`, this properly outputs each track as a seperate file.
This also outputs track info as JSON.

You can convert the output RAW PCM to WAV using sox: `sox -t raw -r 44100 -e signed-integer -L -b 16 -c 2 <file.raw> <out.wav>`