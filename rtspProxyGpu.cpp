/* GStreamer
* Copyright (C) 2019 Mathieu Duponchelle <mathieu@centricular.com>
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/
#include "rtspProxyGpu.hpp"

using json = nlohmann::json;
using namespace std;

#define MAKE_AND_ADD(var, pipe, name, label, elem_name) \
G_STMT_START { \
	if (G_UNLIKELY (!(var = (gst_element_factory_make (name, elem_name))))) { \
		g_print ("Could not create element %s\n", name); \
		goto label; \
	} \
	if (G_UNLIKELY (!gst_bin_add (GST_BIN_CAST (pipe), var))) { \
		g_print ("Could not add element %s\n", name); \
		goto label; \
	} \
} G_STMT_END

#define __CHANGE_RESOLUION
#define DEFAULT_INT 	-1
#define PROF_BASELINE 	0
#define PROF_MAIN 		2
#define PROF_HIGH 		4
#define VIDENC_VARIABLE_BITRATE	0
#define VIDENC_CONSTANT_BITRATE 1
#define IFRAME_MAX 	4294967295
#define IFRAME_MIN 	0
#define MAX_STREAM 10

static gchar *username = NULL;
static gchar *password = NULL;

static int profile = PROF_BASELINE;
static int iframeinterval = DEFAULT_INT;
static int qos = 0;
static int rc_mode = VIDENC_CONSTANT_BITRATE;
static int stream_count = 0;

string uri_str[MAX_STREAM];
gchar * inputCodec_str[MAX_STREAM];
string outputCodec_str[MAX_STREAM];

struct Stream {
	gchar *stream = NULL;
	int bitrate = 0;
	gchar *inputCodec = NULL;
	gchar *outputCodec = NULL;
	int height = 0;
	int width = 0;
};
static Stream transcoding_stream[MAX_STREAM];

static int check_rc(int rc) {
	return !(rc == VIDENC_CONSTANT_BITRATE || rc == VIDENC_VARIABLE_BITRATE);
}

static int check_iframe(int iframe) { 
	return iframe >= IFRAME_MIN && iframe <= IFRAME_MAX;
}

static int check_profile(int prof) {
	return !(prof == PROF_BASELINE || prof == PROF_MAIN || prof == PROF_HIGH);
}

static void enc_pad_added(GstElement * x264enc, GstPad * pad, GstGhostPad * ghost) {
	GstCaps *caps = gst_pad_get_current_caps(pad);
	GstStructure *s = gst_caps_get_structure(caps, 0);

	if (gst_structure_has_name(s, "video/x-h264")) {
		gst_ghost_pad_set_target(ghost, pad);
	}
	gst_caps_unref(caps);
}

void pad_added_cb(GstElement * src, GstPad * srcpad, GstElement * peer) {
	//g_print("Pad added\n");
	GstPad *sinkpad = gst_element_get_static_pad(peer, "sink");

	gst_pad_link(srcpad, sinkpad);

	gst_object_unref(sinkpad);
}


void makeAuth(GstRTSPMediaFactory *factory, GstRTSPServer *server) {
	//Check if username and password is not null, make authentiacte, else 
	if (username == NULL || password == NULL) {
		g_print("Authentiaction is not set!!!\n");
		return;
	}
	GstRTSPAuth *auth;
	GstRTSPToken *token;
	gchar *basic;
	gst_rtsp_media_factory_add_role(factory, "admin",
		GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
		GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
	/* make a new authentication manager */
	auth = gst_rtsp_auth_new();

	/* make admin token */
	token =
		gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
			"admin", NULL);
	basic = gst_rtsp_auth_make_basic(username, password);
	gst_rtsp_auth_add_basic(auth, basic, token);
	g_free(basic);
	gst_rtsp_token_unref(token);
	g_print("Authentiaction already set\n");
	/* set as the server authentication manager */
	gst_rtsp_server_set_auth(server, auth);
	g_object_unref(auth);
}

/* A simple factory to set up our replay bin */
struct _OnvifFactory {
	GstRTSPMediaFactory parent;
};

G_DEFINE_TYPE(OnvifFactory, onvif_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

static void onvif_factory_init(OnvifFactory * factory) {}

gboolean link_element_to_streammux_sink_pad (GstElement *streammux, GstElement *elem, gint index) {
	gboolean ret = FALSE;
	GstPad *mux_sink_pad = NULL;
	GstPad *src_pad = NULL;
	gchar pad_name[16];

	if (index >= 0) {
		g_snprintf (pad_name, 16, "sink_%u", index);
		pad_name[15] = '\0';
	} else {
		strcpy (pad_name, "sink_%u");
	}

	mux_sink_pad = gst_element_get_request_pad (streammux, pad_name);
	if (!mux_sink_pad) {
		g_print ("Failed to get sink pad from streammux");
		goto done;
	}

	src_pad = gst_element_get_static_pad (elem, "src");
	if (!src_pad) {
		g_print ("Failed to get src pad from '%s'", GST_ELEMENT_NAME (elem));
		goto done;
	}
	if (gst_pad_link (src_pad, mux_sink_pad) != GST_PAD_LINK_OK) {
		g_print ("Failed to link '%s' and '%s'", GST_ELEMENT_NAME (streammux), GST_ELEMENT_NAME (elem));
		goto done;
	}
	ret = TRUE;

done:
	if (mux_sink_pad) {
		gst_object_unref (mux_sink_pad);
	}
	if (src_pad) {
		gst_object_unref (src_pad);
	}
	return ret;
}

static GstElement * rtsp_create_element(GstRTSPMediaFactory * factory, const GstRTSPUrl * url) {
	char format[100] = "video/x-raw,format=(string)RGBA,width=(int)%d,height=(int)%d";
	char filter[120];
	gchar *path = NULL;
	int width, height;
	string inputCodec, outputCodec, streamUrl;
	GstElement *parse, *pay, *onvifts, *enc, *dec, *src, *depay, *scale, *buffer;
	GstElement *cap_filter1, *cap_filter;
	GstCaps *caps = NULL, *caps1 = NULL, *convertCaps = NULL;
	GstElement *nvvidconv, *nvvidconv1;
	GstCapsFeatures *feature = NULL;
	GstCapsFeatures *feature1 = NULL;
	GstElement *ret = gst_bin_new(NULL);
	GstElement *pbin = gst_bin_new("pay0");
	GstPad *sinkpad, *srcpad;
	gchar *camId = NULL;
	guint64 timestamp;
	gint bitrate = 0;

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

	string absPath(url->abspath);
	g_print("Abs path: %s\n", url->abspath);

	string stmtSelect = "SELECT cameras.camera_url, cameras.camera_codec, streams.stream_codec, streams.stream_width, streams.stream_height";
	string stmtFromJoin = "FROM streams INNER JOIN cameras ON streams.stream_input_camera = cameras.camera_name WHERE streams.stream_path = '";
	string stmtFull = stmtSelect + stmtFromJoin + absPath + "';";
	res = stmt->executeQuery(stmtFull);

	if (!res->next()) {
		goto fail;
	}
	do {
		streamUrl = res->getString("camera_url");
		inputCodec = res->getString("camera_codec");
		outputCodec = res->getString("stream_codec");
		width = res->getInt("stream_width");
		height = res->getInt("stream_height");
	} while (res->next());

	// Make video part
	// rtspsrc ! rtph264depay ! nvv4l2decoder ! nvv4l2h264enc ! rtph264pay
	if (inputCodec.find("H.264") != string::npos && outputCodec.find("H.264") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph264depay", fail, "depay");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h264enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph264pay", fail, NULL);

	} else if (inputCodec.find("H.265") != string::npos && outputCodec.find("H.265") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph265depay", fail, "depay");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h265enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph265pay", fail, NULL);

	} else if (inputCodec.find("H.264") != string::npos && outputCodec.find("H.265") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph264depay", fail, "depay");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h265enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph265pay", fail, NULL);

	} else if (inputCodec.find("H.265") != string::npos && outputCodec.find("H.264") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph265depay", fail, "depay");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h264enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph264pay", fail, NULL);

	} else {
		goto fail;
	}

	g_signal_connect(src, "pad-added", G_CALLBACK(pad_added_cb), depay);

	if (streamUrl.empty()) {
		g_print("Error: Stream is NULL\n");
	} else {
		g_print("Connect to %s, codec: %s, ", streamUrl.c_str(), inputCodec.c_str());
	}
	g_object_set(src, "location", (gchar*) streamUrl.c_str(), NULL);
	g_object_set(G_OBJECT (src), "latency", 1000, NULL);
	g_object_set(G_OBJECT (src), "drop-on-latency", TRUE, NULL);
	
	gst_bin_add(GST_BIN(ret), pbin);

	// Set bitrate for resolution
	if (height <= 360) bitrate = 1;
	else if (height <= 576) bitrate = 2;
	else if (height <= 720) bitrate = 3;
	else if (height <= 1080) bitrate = 4;
	else bitrate = 5;

	if (bitrate > 0) {
		g_print("bitrate: %d Mbps\n", bitrate);
		g_object_set(enc, "bitrate", bitrate * 1000000, NULL);
	} else {
		g_print("Error: Invalid value of bitrate\n");
		gst_object_unref(ret);
		ret = NULL;
		goto done;
	}
	
	// }else {
	// 	g_print("bitrate: %d Mb\n", DEFAULT_BIT_RATE_PROFILE/1000000);
	// 	g_object_set(enc, "bitrate", DEFAULT_BIT_RATE_PROFILE, NULL);
	// }
	// if (rc_mode != VIDENC_CONSTANT_BITRATE && check_rc(rc_mode))
	// 	g_print("Set control-rate to: %d\n", rc_mode);
	// 	g_object_set(enc, "control-rate", rc_mode, NULL);

	// if (profile != PROF_BASELINE && check_profile(profile)){
	// 	g_print("Set profile to: %d\n", profile);
	// 	g_object_set(enc, "profile", profile, NULL);
	// }
	// if (iframeinterval != DEFAULT_INT && check_iframe(iframeinterval)){
	// 	g_print("Set iframeinterval to: %d\n", iframeinterval);
	// 	g_object_set(enc, "iframeinterval", iframeinterval, NULL);
	// }

#ifdef __CHANGE_RESOLUION

	if (width > 0 && height > 0) {
		printf("Set scale width: %d, height: %d\n", width, height);

		MAKE_AND_ADD(scale, pbin, "nvstreammux", fail, NULL);
		g_object_set(G_OBJECT(scale), "width", width, NULL);
		g_object_set(G_OBJECT(scale), "height", height, NULL);
		g_object_set(G_OBJECT(scale), "batch-size", 1, NULL);

		if (!link_element_to_streammux_sink_pad(scale, dec, 0)) {
			g_print("Link steammux false\n");
			goto fail;
		}	
		if (!gst_element_link_many(scale, enc, pay ,NULL)) {
			g_print("Link filter false\n");
			goto fail;
		}
	} else {
		if (!gst_element_link_many(dec, enc, pay , NULL)) {
			g_print("Link dec enc false\n");
			goto fail;
		}
	}
	
	if (!gst_element_link_many(depay, dec, NULL)){
		g_print("Link many scale false\n");
		goto fail;
	}

#else
	if (!gst_element_link_many(depay, dec, enc, pay, NULL)) {
		g_print("Link many false\n");
		goto fail;
	}

#endif
	srcpad = gst_element_get_static_pad(pay, "src");
	gst_element_add_pad(pbin, gst_ghost_pad_new("src", srcpad));
	gst_object_unref(srcpad);

done:
	g_free(path);
	delete res;
	delete stmt;
	delete conn;
	return ret;

fail:
	g_print("Error: Can not link to stream\n");
	gst_object_unref(ret);
	ret = NULL;
	goto done;
}

static void onvif_factory_class_init(OnvifFactoryClass * factory_class) {
	GstRTSPMediaFactoryClass *mf_class = GST_RTSP_MEDIA_FACTORY_CLASS(factory_class);
	//mf_class->create_element = onvif_factory_create_element;
	mf_class->create_element = rtsp_create_element;
}
 
GstRTSPMediaFactory * onvif_factory_new(void) {
	GstRTSPMediaFactory *result;

	result =
		GST_RTSP_MEDIA_FACTORY(g_object_new(onvif_factory_get_type(), NULL));

	return result;
}

/* Now do not need to use, it's past
static void on_request(GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server, gpointer p_user)
{
	GstRTSPClientClass *klass = GST_RTSP_CLIENT_GET_CLASS(client);
	gchar * path = klass->make_path_from_uri(client, ctx->uri);

	GstRTSPMountPoints *mountPoints = gst_rtsp_server_get_mount_points(server);
	GstRTSPMediaFactory *factory = NULL;

	factory = gst_rtsp_mount_points_match(mountPoints, path, NULL);
	if (!factory)
	{
		g_print("New file requset\n");
	}
}*/


static void on_client_new_session(GstRTSPClient * self, GstRTSPSession * object, gpointer user_data) {
	g_print("New session\n\n");
	gst_rtsp_session_set_timeout(object, 20);
}

// Handle when client connected
void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data) {
	g_print("Client connected\n");
	g_signal_connect_object(client, "new-session", G_CALLBACK(on_client_new_session), NULL, G_CONNECT_AFTER);

	//g_signal_connect_object(client, "options-request", G_CALLBACK(on_request), NULL, G_CONNECT_AFTER);

	//GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);
	//GstRTSPUrl *url = gst_rtsp_connection_get_url(conn);
	//g_print("Abs path: %s\n", gst_rtsp_url_get_request_uri(url));
}


int parse_stream_from_json (json jsonData) {
	/* Set up database */
	sql::Driver *driver;
	sql::Connection *conn;
	sql::Statement *stmt;
	sql::ResultSet *res;

	// Create a connection
	driver = get_driver_instance();
	conn = driver->connect("localhost", "hoangnv", "bkcs2022");
	conn->setSchema("transcoding"); // database name
	stmt = conn->createStatement();
	stmt->execute("DELETE FROM streams"); // Remove later

	/* Access fields from JSON object */
	unsigned long long pathIdRange = PATH_ID_UPPER_BOUND - PATH_ID_LOWER_BOUND + 1;
	for (auto &arr: jsonData["streams"]){
		int streamWidth, streamHeight;
		string streamPath, streamInputCamera, streamCodec;

		// Parse stream input camera name
		streamInputCamera = arr.at("camera_name").get<string>();
		res = stmt->executeQuery("SELECT camera_url FROM cameras WHERE camera_name = '" + streamInputCamera + "'");
		if (!res->next()) {
			cerr << "\tCamera not found" << endl;
			continue;
		}

		// Parse output codec
		streamCodec = arr.at("codec").get<string>();
		if (streamCodec.empty()) {
			g_printerr("Output video codec not found!\n\n");
			continue;
		}

		// Parse resolution
		try {
			streamWidth = stoi(arr.at("width").get<string>());
		} catch (const exception& e) {
			cout << e.what() << ". Using camera resolution setting.\n";
		}

		try {
			streamHeight = stoi(arr.at("height").get<string>());
		} catch (const exception& e) {
			cout << e.what() << ". Using camera resolution setting.\n";
		}

		// Generate path
		unsigned long long pathId = rand() % pathIdRange + PATH_ID_LOWER_BOUND;
		streamPath = "/" + streamInputCamera + "/" + to_string(pathId);
		
		// Add to database
		stmt->execute("INSERT INTO streams(stream_path, stream_input_camera, stream_width, stream_height, stream_codec) VALUES ('" 
				+ streamPath + "', '" + streamInputCamera + "', " + to_string(streamWidth) + ", " + to_string(streamHeight) + ", '" + streamCodec + "');");

		stream_count++;
	}


	if (stream_count <= 0) {
		g_printerr("NO INPUT STREAMS FOUND!\n");
		return -1;
	}

	// res = stmt->executeQuery("SELECT cameras.camera_url, cameras.camera_codec, streams.stream_codec, streams.stream_width, streams.stream_height FROM streams INNER JOIN cameras ON streams.stream_input_camera = cameras.camera_name;");

	// if (!res->next()) {
	// 	cerr << "Parsing setting file failed!" << endl;
	// }
	// do {
	// 	cout << "URL: '" << res->getString("camera_url") 
	// 		<< ", input codec: " << res->getString("camera_codec") 
	// 		<< ", output codec: " << res->getString("stream_codec") 
	// 		<< ", resolution: " << res->getInt("stream_width") << "x" << res->getInt("stream_height")
	// 		<< endl;
	// } while (res->next());


	// Free MySQL variables
	delete res;
	delete stmt;
	delete conn;

	return stream_count;
}