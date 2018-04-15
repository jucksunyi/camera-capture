1: for staic-capture.c，it just capture one capture, you can compile it with normal gcc .the execute the command is  ./ccccc /dev/video0.
the you will find video.yuv int the directory /tmp, then copy it to windows directory, the use software YUVPlayer open it

2: for dynamic-capture-1.c,it can capture then display in the HDMI, you can compile it with 
"source /opt/poky/2.1.2/environment-setup-aarch64-poky-linux   
$CXX -o camera  camera.c -lopencv_highgui -lopencv_imgproc -lopencv_core -lopencv_video -lavcodec -lavfilter -lavdevice -lavutil"
the execute the command is  "./camera /dev/video0".

3: for dynamic-capture-4.c,i it for 4 cameras automatic switching.
