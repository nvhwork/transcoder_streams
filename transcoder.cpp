#include <iostream>
#include <fstream>
#include <cstdlib>
#include <vector>
#include <glib.h>
#include <gio/gnetworking.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/rtsp/gstrtspconnection.h>
#include "rtspProxyGpu.hpp"

#define GST_CAT_DEFAULT (onvif_server_debug)

GST_DEBUG_CATEGORY_STATIC(onvif_server_debug);

using json = nlohmann::json;
using namespace std;

static gchar *port = NULL;
static gchar *stream_config_file = NULL;

json get_stream_from_input(int argc, char *argv[]) {
	GOptionContext *optctx;
	GError *error = NULL;
	GOptionEntry entries[] = {
		//{ "rtsp-port", 'p', 0, G_OPTION_ARG_STRING, &port, "RTSP server port", NULL },
		//{ "stream-1", 's', 0,  G_OPTION_ARG_STRING, &stream1, "rtsp stream to proxies 1", NULL },
		//{ "stream-2", 'l', 0,  G_OPTION_ARG_STRING, &stream2, "rtsp stream to proxies 2", NULL },		
		//{ "username", 'u', 0, G_OPTION_ARG_STRING, &username, "Username for authentication", NULL },
		//{ "password", 'd', 0, G_OPTION_ARG_STRING, &password, "Password for authentication", NULL },
		{ "config", 'c', 0,  G_OPTION_ARG_STRING, &stream_config_file, "Config file for RTSP streaming", NULL },
			
		#ifdef __CHANGE_RESOLUION // videoscale option
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
		{ NULL }
	};
	optctx = g_option_context_new("RTSP PROXY");
	g_option_context_add_main_entries(optctx, entries, NULL);
	g_option_context_add_group(optctx, gst_init_get_option_group());

	if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
		g_printerr("Error parsing options: %s\n", error->message);
		g_option_context_free(optctx);
		g_clear_error(&error);
		return NULL;
	}

	g_option_context_free(optctx);
	if (stream_config_file == NULL){
		g_print("Config file not set.\n");
		return NULL;
	}

	// Read the JSON file
	ifstream f(stream_config_file, ifstream::in);
    json jsonData; // Create JSON object
    f >> jsonData; // Initialize JSON object with what was read from file

	return jsonData;
}

int main(int argc, char *argv[]) {
	int num_of_streams;
	vector<string> paths;
	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	gchar *service;

	// json jsonData = get_stream_from_input(argc, argv);
	// if (jsonData == NULL) return -1;

	// string str = jsonData.at("port").get<string>();
	// vector<char> chars(str.c_str(), str.c_str() + str.size() + 1u);
	// port = &chars[0];

	// if (port == 0) {
	// 	g_printerr("RTSP Port not setted.\n");
	// 	return -1;
	// }
	
	// num_of_streams = parse_stream_from_json(jsonData);
	// if (num_of_streams <= 0) {
	// 	return -1;
	// }

	GST_DEBUG_CATEGORY_INIT(onvif_server_debug, "onvif-server", 0, "ONVIF server");

	loop = g_main_loop_new(NULL, FALSE);
	
	server = gst_rtsp_onvif_server_new();
	gst_rtsp_server_set_service(server, "8550");
	mounts = gst_rtsp_server_get_mount_points(server);

	// Create facory
	factory = onvif_factory_new();
	gst_rtsp_media_factory_set_media_gtype(factory, GST_TYPE_RTSP_ONVIF_MEDIA);
	gst_rtsp_server_set_address(server, "0.0.0.0");
	// if (username && password) {
	// 	makeAuth(factory, server);
	// }
	
	// Set up database
	sql::Driver *driver;
	sql::Connection *conn;
	sql::Statement *stmt;
	sql::ResultSet *res;

	// Create a connection
	driver = get_driver_instance();
	conn = driver->connect("localhost", "hoangnv", "bkcs2022");
	conn->setSchema("transcoding"); // database name
	stmt = conn->createStatement();
	res = stmt->executeQuery("SELECT stream_path FROM streams;");
	while (res->next()) {
		string pathStr = res->getString(1);
		gst_rtsp_mount_points_add_factory(mounts, (const gchar*) pathStr.c_str(), factory);
		paths.push_back(pathStr);
	}

	delete res;
	delete stmt;
	delete conn;

	g_object_unref(mounts);
	g_signal_connect(server, "client-connected", G_CALLBACK(on_client_connected), NULL);
	gst_rtsp_server_attach(server, NULL);

	service = gst_rtsp_server_get_service(server);

	g_print("\nStream ready at:\n");
	for (int i = 0; i < paths.size(); i++) {
		cout << "rtsp://0.0.0.0:" << service << paths[i] << endl;
	}
	cout << endl;

	g_free(service);
	g_main_loop_run(loop);
	return 0;
}
