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

int main(int argc, char *argv[]) {
	int num_of_streams;
	vector<string> paths;
	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	gchar *service;
	GError *error = NULL;
	GOptionContext *optctx = g_option_context_new("RTSP PROXY");

	// Set up context
	g_option_context_add_group(optctx, gst_init_get_option_group());
	if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
		g_printerr("Error parsing options: %s\n", error->message);
		g_option_context_free(optctx);
		g_clear_error(&error);
		return 0;
	}
	g_option_context_free(optctx);

	GST_DEBUG_CATEGORY_INIT(onvif_server_debug, "onvif-server", 0, "ONVIF server");
	loop = g_main_loop_new(NULL, FALSE);
	
	server = gst_rtsp_onvif_server_new();
	gst_rtsp_server_set_service(server, "8550");
	mounts = gst_rtsp_server_get_mount_points(server);

	// Create facory
	factory = onvif_factory_new();
	gst_rtsp_media_factory_set_media_gtype(factory, GST_TYPE_RTSP_ONVIF_MEDIA);
	gst_rtsp_media_factory_set_publish_clock_mode(factory, GST_RTSP_PUBLISH_CLOCK_MODE_CLOCK_AND_OFFSET);
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
