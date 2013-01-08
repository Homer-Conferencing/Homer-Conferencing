# Homer Conferencing software
Homer Conferencing (short: Homer) is a free SIP spftphone with advanced audio and video support. The software is available for Windows, Linux and OS X. Homer was originally developed as tool for video conferences. Over the years the functions were extended for additional application areas. Hence, Homer can now be used for the following 4 application areas video conferencing, streaming, recording, screencasting. 

## Video conferencing
By the help of Homer, one is able to contact other participants via the conference protocol SIP both in the local and global network. During such a session with other participants, chats as well as audio and video can be exchanged. Homer enables to hear and see the other participant, simultaneously. This is also valid for the opposite direction. Moreover, local audio and video data (files) can be transmitted in real-time to another participant. This function be used, e.g., for broadcasting movies or the local desktop picture. Each participant can send - depending on the software settings - comments and questions to all or single participants. of such a conference. For this kind of conferences an unlimited amount of participants is possible - a server is not needed. For contacting each other one must know solely either the current IP address or the DNS name of each desired participant. Additionally to this central mode, Homer also supports the often used central mode based on a SIP server (pbx box). Independent from the selected mode, Homer can exchange messages as well as audio and video data with each participant.

## Streaming
If one wants to show parts of a local video to a participant simultaneously to an already running video conference in order to discuss its content, transmitting of single A/V streams separately has to be possible. For this purpose, Homer enables sending single A/V data in real-time (also without a simultaneously running video conference) via separate transmission channels. Infrastructure is not needed for this application. The remote station can directly receive the data and use it as input for further processing. In general, Homer uses known codecs for transmission for both video (H.261, H.263(+), H.264, MPEG1/2/4,..) and audio data (MP3, aLAW, uLAW,..). The applied quality level for the A/V transmission can be adjusted by the help of the GUI. It is possible to transmit A/V stream both with very low and very high quality (e.g. HDTV). The actual possibilities are limited by the used hardware. But the software architecture of Homer is constructed for parallelization of internal processing steps. In order to enable high quality settings, as much as possible of the available CPU cores are used simultaneously.  

Homer supports for the transmission of A/V stream basically both IPv4 and IPv6. The protocol version is selected automatically. For the transmission of A/V data, Homer does not only support the classic transport protocols TCP and UDP, but also alternative protocols, e.g., SCTP and UDP-Lite, are supported. The possibilities in this context are limited by the used operating system and the therefore available functions. Additionally to standard application, Homer is also used for experiments by some developers, and it therefore provides in the so called developer/debug mode some extra functions. For example, one can assign additional transmission requirements to an A/V stream. They could include QoS parameters or define that transmitted data has to be delivered to receiver side without any data loss. Homer is able to signal such transmission requirements towards the network. The possible impacts of such extra requirements depend on the used network API and can vary accordingly. Additionally, by the help of the GUI the maximum packet size of an A/V stream can be defined and therefore the behavior during packet creation can be influenced. However, the officially available release versions do not include an explicit support of experiments. The are limited to the standard case, which uses the known Berkeley sockets as interface towards the network. Only the packet size limitation is included and can be used for the optimization of the transmission quality. 

## Recording
For a delayed insertion of contents or for repeated playback, it is necessary to be able to record A/V streams locally on a hard disc or similar. For this purpose, each single incoming audio and video stream (from a file, from the network, from a device) can be recorded locally to the disc and therefore made available for a delayed post processing. For example, it is possible to capture the video from a webcam in real-time and record it directly to disc or similar. Alternatively, one can use this function to record a per network received A/V stream directly. For video recording known file formats (avi, m4v, mov, mp4, 3gp,..) are supported. Additionally, for audio recording also known file formats (mp3, wav,..) are supported.

## Screencasting
For the demonstration of processes on the desktop it would be interesting to be able to either record or to stream via network a live video of the activities on the desktop. For both applications, the needed functions are provided by the software. For example, Homer can be used for recording a screencast or for presenting software processes via a live broadcast of the desktop picture. Like other video sources, the desktop picture can also be used during a video conference and therefore being showed to all participants for discussing in real-time. However, a non wished observation or control of the local desktop by a remote station are not possible.

## Minimum system requirements
* 32/64 bit processor with 2 Ghz (4 cores with 3 Ghz are recommended for high qualitative video streaming), the OS X version can only be used on 64 bit prozessors
* for Windows: Windows XP SP3 (Windows 7 recommended)
* for Linux: Linux kernel 2.6 (Linuxkernel >= 3.1 recommended)
* for Mac Hardware: OS X Snow Leopard (OS X Lion recommended)


# The project

## Find further information

* Web site: http://www.homer-conferencing.com/
* Follow Homer on Facebook: http://www.facebook.com/homerconferencing
* Mailing list: https://lists.sourceforge.net/lists/listinfo/homer-conf-users (You have to be subscribed to the mailing list before you are allowed to post to the list!)


## Contribute to the project

* Report a bug: https://github.com/Homer-Conferencing/Homer-Conferencing/issues
* Source code: https://github.com/Homer-Conferencing/Homer-Conferencing
