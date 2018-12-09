############################
#                          #
# README                   #
#                          #
#    Mono Chioh 08/16/2016 #
#                          #
############################


The files here are motion sample video clips that include h264 I and P frames.

There's also a hardcoded timer on each and every video clip for better experience of testing.

Please run `./AVAPIs2_Server [UID] video_multi/[title].multi` to use these clips.

Audio/video files beethoven* and joplin* are especially good for testing audio/video synchronization.

The clips with the same prefix are extracted from the very same multimedia source.

By running `./AVAPIs2_Server [UID] video_multi/beethoven.multi audio_raw/beethoven_8k_16bit_mono.raw` for instance,
you will be able to clearly observe the audio/video frame presentation time, so as to tune the sychronization algorithm.

In addition, both of these two sets have two clips with different resolution -- for example, beethoven.multi (720p),
and beethoven_360.multi (360p).

The developers may choose any of those as they wish.