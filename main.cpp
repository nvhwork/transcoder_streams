#include <iostream>
#include <fstream>
#include <cstdlib>
#include <glib.h>
#include <gio/gnetworking.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspconnection.h>
#include "rtspProxyGpu.hpp"

#define __ENABLE_OPTIONS
#define GST_CAT_DEFAULT (onvif_server_debug)

GST_DEBUG_CATEGORY_STATIC(onvif_server_debug);

using json = nlohmann::json;
using namespace std;

static gchar *port = NULL;
static gchar *stream_config_file =NULL;

int main(int argc, char *argv[]) {
	int num_of_streams;
	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	GOptionContext *optctx;
	GError *error = NULL;
	gchar *service;
	GOptionEntry entries[] = {
		//{ "rtsp-port", 'p', 0, G_OPTION_ARG_STRING, &port, "RTSP server port", NULL },
		//{ "stream-1", 's', 0,  G_OPTION_ARG_STRING, &stream1, "rtsp stream to proxies 1", NULL },
		//{ "stream-2", 'l', 0,  G_OPTION_ARG_STRING, &stream2, "rtsp stream to proxies 2", NULL },		
		//{ "username", 'u', 0, G_OPTION_ARG_STRING, &username, "Username for authentication", NULL },
		//{ "password", 'd', 0, G_OPTION_ARG_STRING, &password, "Password for authentication", NULL },
		{ "config", 'c', 0,  G_OPTION_ARG_STRING, &stream_config_file, "Config file for RTSP streaming", NULL },
#ifdef __ENABLE_OPTIONS
			//videoscale option
		#ifdef __CHANGE_RESOLUION
		//{ "width", 'w', 0, G_OPTION_ARG_INT, &width, "Resize: width", NULL },
		//{ "height", 'h', 0, G_OPTION_ARG_INT, &height, "Resize: height", NULL },
		#endif
			//x264enc option
		//{ "qos", 'q', 0, G_OPTION_ARG_INT, &qos, "encoder: QoS", NULL },
		//{ "profile", 'f', 0, G_OPTION_ARG_INT, &profile, "encoder: profile", NULL },
		//{ "bitrate-1", 0, 0, G_OPTION_ARG_INT, &bitrate1, "encoder: bitrate profile ( 1 or 2 ) of output stream", NULL },
		//{ "bitrate-2", 0, 0, G_OPTION_ARG_INT, &bitrate2, "encoder: bitrate profile ( 1 or 2 ) of output stream", NULL },
		//{ "rc-mode", 'c', 0, G_OPTION_ARG_INT, &rc_mode, "encoder: bitrate mode", NULL },
		//{ "iframeinterval", 'i', 0, G_OPTION_ARG_INT, &iframeinterval, "encoder: iframe interval", NULL },

#endif
		{ NULL }
	};
	optctx = g_option_context_new("RTSP PROXY");
	g_option_context_add_main_entries(optctx, entries, NULL);
	g_option_context_add_group(optctx, gst_init_get_option_group());

	if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
		g_printerr("Error parsing options: %s\n", error->message);
		g_option_context_free(optctx);
		g_clear_error(&error);
		return -1;
	}
	if (stream_config_file == NULL){
		g_print("Config file not setted.\n");
		return 1;
	}

	// Read the JSON file
	ifstream f(stream_config_file, ifstream::in);
    json jsonData; // Create JSON object
    f >> jsonData; // Initialize JSON object with what was read from file

	string str = jsonData.at("port").get<string>();
	vector<char> chars(str.c_str(), str.c_str() + str.size() + 1u);
	port=&chars[0];

	if (port == 0) {
		g_printerr("RTSP Port not setted.\n");
		return 1;
	}
	
	num_of_streams = parse_streams_from_json(jsonData);
	if (num_of_streams <= 0) {
		return 1;
	}

	g_option_context_free(optctx);

	GST_DEBUG_CATEGORY_INIT(onvif_server_debug, "onvif-server", 0, "ONVIF server");

	loop = g_main_loop_new(NULL, FALSE);
	
	server = gst_rtsp_onvif_server_new();
	gst_rtsp_server_set_service(server, port);
	mounts = gst_rtsp_server_get_mount_points(server);

	// Create facory
	factory = onvif_factory_new();
	gst_rtsp_media_factory_set_media_gtype(factory, GST_TYPE_RTSP_ONVIF_MEDIA);
	gst_rtsp_server_set_address(server, "0.0.0.0");
	// if (username && password) {
	// 	makeAuth(factory, server);
	// }
	
	for(int i = 1; i <= num_of_streams; i++){
		char init_uri[] = "/stream-";
		string s = to_string(i);
		char *index =(char*) s.c_str();
		gst_rtsp_mount_points_add_factory(mounts, (const gchar*)strcat(init_uri,index), factory);
	}

	g_object_unref(mounts);
	g_signal_connect(server, "client-connected", G_CALLBACK(on_client_connected), NULL);
	gst_rtsp_server_attach(server, NULL);

	service = gst_rtsp_server_get_service(server);
	g_print("\nStream ready at:\n");
	for (int i = 1; i <= num_of_streams; i++) {
		char init_uri[] = "/stream-";
		string s = to_string(i);
		char *index =(char*) s.c_str(); 
		cout << "rtsp://0.0.0.0:" << service << strcat(init_uri, index) << endl;
	}
	cout << endl;
	g_free(service);
	g_main_loop_run(loop);
	return 0;
}
