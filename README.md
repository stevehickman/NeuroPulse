The goal of this project is to develop a modular HW/SW platform that provides multiple simultaneous neurological stimulation modalities + measurement of the effect of applying those modalities, ultimately leading to a tool that can dynamically adapt to brain state and provide positive stimulus to move / keep the brain in good health.

Claude.ai is being used as much as possible to further these goals. CLAUDE.md contains its understanding of the design and its current state.

The /docs directory contains design documents. Start reading there.
The /editscripts directory contains generated scripts that Claude used to modify documents. The content of this directory is probably not for human consumption.
The /app directory contains code for iOS and watchOS devices
The /firmware directory contains the code that runs the hardware

This all started with the following prompts to Claude:

I want to design a product that has all the following features: 
1) Photobiomodulation (650,850, and 1170 nm) for the brain;
2) vagus nerve stimulation;
3) transcranial magnetic stimulation;
4) qEEG; 5) Transcranial alternating current stimulation. All of these should be individually programmable.
   
It should be possible to combine any set of features as desired. All individual features and features combinations should be controllable via a software app that runs on standard platforms (Mac, PC, phone, tablet)


