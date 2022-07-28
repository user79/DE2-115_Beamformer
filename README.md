# DE2-115_Beamformer Design

This is a simple narrow-band 6-microphone beamformer project implemented on the Terasic DE2-115 (Intel/Altera Cyclone IV FPGA) as a demonstration.  

It uses the Texas Instruments Tiva board (TM4C123G) for its 6-channel ADC (12-bit, run at ~48ksps) as well as the Wolfson WM8731 codec chip on the DE2-115 board for audio input and output.  The interface with the WM8731 is based on the code provided by Altera in its University Program "Laboratory Exercise 12" (previously freely available to download, the lab document is currently available here https://fpgacademy.org/courses.html , I am removing the Altera laboratory code from this posting because I'm not sure what its license is... it may not be open-source).

This demo was designed to use simple, easily-testable numbers, rather than pushing the performance to the limit. It has an option of using a "straight-ahead" beam (where the inputs to the microphones are simply summed in the FPGA) (a far-field flat-wavefront model is assumed), or a ~43 degree off-axis beam (where the signals to the microphones are delayed by 3n samples (where n=1,2,3...6) at a sampling rate of 48ksps).

A simple handshake was designed, so that the DE2 sends a "ready" pulse to the Tiva, then the Tiva puts its 6 most recent audio samples onto the 12-bit bus and pulses a "serial clock" line 6 times to the DE2.  The DE2 loads these samples into a buffer and shifts them through the appropriate delays, before adding and feeding the result back into the WM8731 codec for line-out listening or computer analysis.

The microphones used were MAX4466-op-amp-based electret breakout boards, https://www.adafruit.com/product/1063 , mounted 3.1 cm apart on a solderless breadboard in a straight line.

In this simple demo, samples could only be delayed by discrete amounts (each sample time of delay was 1/48000 seconds, which corresponded to a physical distance of the audio wavefront traveling 0.71 cm, assuming that the speed of sound was 340 meters/second).  Thus, a delay of 1 sample (for the first microphone, 2 samples for the second microphone, etc) corresponds to a beam angle of 13.21 degrees.  Likewise, a 2n samples delay corresponded to 27.19 degrees, and 3 samples corresponded to a beam angle of 43.27 degrees.  This was chosen based on a presumed convenient ease of demonstration (0 degrees versus ~45 degrees).  The presumed ideal frequency to test at was 914 Hz, based on the assumption that ideal rejection of off-angle signals would be seen when the peak of the wavefront at the first microphone aligned with the trough of the wavefront at the last microphone, providing theoretically perfect cancellation.

Additional supporting design documents can be found in the PDF and Excel files.

# Testing results

Initial testing used a sinusoidal audio tone from a cell phone held at the same radial distance (approximately 50 cm) and at different angles from the microphone array.  As expected, some reduction in the output amplitude was noticed off-axis (for both the straight-ahead zero-degree switch selection and the 43 degree switch selection), but it was not as pronounced as expected.  Recordings were also taken of the microphones and fed into Audacity for examination.  Substantial noise was found, as well as some distortion (compared to the much cleaner signal coming from the WM8731 audio codec on board the DE2).  This is to be expected, and more work remains to be done for further quantitative testing and optimization of the signal quality.

# Summary

I wanted to share this demo to provide the community (especially younger students) with these ideas, even though it is not yet as effective as a professional boardroom beamformer.  

Further improvements could include adding more microphone channels at various larger separation distances, splitting the audio into several narrow frequency bands, and applying the appropriate delays for each band, to avoid aliasing and phase-overlapping problems.  Once the bands are individually appropriately delayed, they can be added together again to produce the final output.

Further signal processing can also be applied to clean up the signal.