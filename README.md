# chromatracker

A [music tracker](https://en.wikipedia.org/wiki/Music_tracker) app for Android and (eventually) iOS. This is a work in progress and has gone through multiple iterations.
The [original version](https://github.com/vanjac/chromatracker/tree/old-version) used SDL and custom module loading / playback code, the current version uses the Android
SDK, and I am currently rewriting it again to use [Qt](https://www.qt.io/) and [libopenmpt](https://lib.openmpt.org/libopenmpt/) for cross-platform compatibility.

My goal is to apply the design/philosophy of trackers to a new mode, with a streamlined mobile interface that retains the capability and efficiency of a tracker.

I documented my two attempts to design a custom module format on the [wiki](https://github.com/vanjac/chromatracker/wiki), before I decided to switch to the established [IT](https://wiki.openmpt.org/Manual:_Module_formats#The_Impulse_Tracker_format_.28.it.29) format.
