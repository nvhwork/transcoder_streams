g++ -std=c++11 -o transcoder rtspProxyGpu.cpp validateUrl.cpp -Wno-deprecated-declarations -pthread `pkg-config --libs --cflags gstreamer-1.0 glib-2.0 gobject-2.0 gstreamer-rtsp-server-1.0 gstreamer-pbutils-1.0`