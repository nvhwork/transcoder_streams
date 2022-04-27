#include <iostream>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspconnection.h>
#include "json.hpp"

using json = nlohmann::json;
using namespace std;

int parse_streams_from_json (json jsonData);
void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data);
GstRTSPMediaFactory * onvif_factory_new(void);
