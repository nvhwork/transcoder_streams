
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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
* Boston, MA 02110-1301, USA.
*/
//#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "json.hpp"
#include <fstream>
#include <cstdlib>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>
#include "server0audio.hpp"
#include "validateUrl.hpp"
#include <glib.h>
#include <gst/gst.h>
#include <gio/gnetworking.h>
#include <gst/rtsp/gstrtspconnection.h>

using json = nlohmann::json;
using namespace std;

GST_DEBUG_CATEGORY_STATIC(onvif_server_debug);
#define GST_CAT_DEFAULT (onvif_server_debug)
#define __ENABLE_OPTIONS
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

#define FIXED_BIT_RATE 1228000
#define BIT_RATE_PROFILE_1 1000000
#define BIT_RATE_PROFILE_2 2000000
#define BIT_RATE_PROFILE_3 3000000
#define DEFAULT_BIT_RATE_PROFILE 4000000
#define MAX_STREAM 10


static gchar *port = NULL;
static gchar *username = NULL;
static gchar *password = NULL;
static gchar *stream1 = NULL;
static gchar *stream2 = NULL;
static gchar *stream_config_file =NULL;


static int bitrate1 = 1;
static int bitrate2 = 1; 
static int profile = PROF_BASELINE;
static int iframeinterval = DEFAULT_INT;
static int qos = 0;
static int rc_mode = VIDENC_CONSTANT_BITRATE;
static int stream_count = 0;

string uri_str[MAX_STREAM];
gchar * input_codec_str[MAX_STREAM];
string output_codec_str[MAX_STREAM];
struct Stream
{
	gchar *stream = NULL;
	int bitrate = 0;
	gchar *inputCodec = NULL;
	gchar *outputCodec = NULL;
	int height = 0;
	int width = 0;
};

static Stream transcoding_stream[MAX_STREAM];
static int check_rc(int rc){
	return !(rc == VIDENC_CONSTANT_BITRATE || rc == VIDENC_VARIABLE_BITRATE);
}

static int check_iframe(int iframe){ 
	return iframe >= IFRAME_MIN && iframe <= IFRAME_MAX;
}

static int check_profile(int prof){
	return !(prof == PROF_BASELINE || prof == PROF_MAIN || prof == PROF_HIGH);
}


static void
enc_pad_added(GstElement * x264enc, GstPad * pad, GstGhostPad * ghost)
{
	GstCaps *caps = gst_pad_get_current_caps(pad);
	GstStructure *s = gst_caps_get_structure(caps, 0);

	if (gst_structure_has_name(s, "video/x-h264")) {
		gst_ghost_pad_set_target(ghost, pad);
	}
	gst_caps_unref(caps);
}

void
pad_added_cb(GstElement * src, GstPad * srcpad, GstElement * peer)
{
	//g_print("Pad added\n");
	GstPad *sinkpad = gst_element_get_static_pad(peer, "sink");

	gst_pad_link(srcpad, sinkpad);

	gst_object_unref(sinkpad);
}


void makeAuth(GstRTSPMediaFactory *factory, GstRTSPServer *server)
{
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
struct _OnvifFactory
{
	GstRTSPMediaFactory parent;
};

G_DEFINE_TYPE(OnvifFactory, onvif_factory, GST_TYPE_RTSP_MEDIA_FACTORY);

static void
onvif_factory_init(OnvifFactory * factory)
{
}

gboolean
link_element_to_streammux_sink_pad (GstElement *streammux, GstElement *elem,
    gint index)
{
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
    g_print ("Failed to get src pad from '%s'",
                        GST_ELEMENT_NAME (elem));
    goto done;
  }
  if (gst_pad_link (src_pad, mux_sink_pad) != GST_PAD_LINK_OK) {
    g_print ("Failed to link '%s' and '%s'", GST_ELEMENT_NAME (streammux),
        GST_ELEMENT_NAME (elem));
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

static GstElement *
rtsp_create_element(GstRTSPMediaFactory * factory,
	const GstRTSPUrl * url)
{
	char format[100] = "video/x-raw,format=(string)RGBA,width=(int)%d,height=(int)%d";
	char filter[120];
	gchar *path = NULL;
	string input_codec, output_codec;
	GstElement *parse, *pay, *onvifts, *enc, *dec, *src, *depay, *scale, *buffer;
	GstElement *cap_filter1,*cap_filter;
	GstCaps *caps = NULL, *caps1 = NULL, *convertCaps = NULL;
	GstElement *nvvidconv,*nvvidconv1;
	GstCapsFeatures *feature = NULL;
	GstCapsFeatures *feature1 = NULL;
	GstElement *ret = gst_bin_new(NULL);
	GstElement *pbin = gst_bin_new("pay0");
	GstPad *sinkpad, *srcpad;
	gchar *camId = NULL;
	guint64 timestamp;
	gchar* streamUrl;
	gint bitrate =0 ;
	int stream_index=-1;
	int width, height;

	g_print("Abs path: %s\n", url->abspath);
	
	for(int i=1;i<=stream_count;i++){
		char init_uri[] ="/stream-";
		string s = to_string(i);
		char *index =(char*) s.c_str();
		if(!g_strcmp0(url->abspath, (const gchar*)strcat(init_uri,index))) stream_index = i-1;
	}
	if(stream_index<0) goto fail;

	input_codec = string(transcoding_stream[stream_index].inputCodec);
	output_codec = string(transcoding_stream[stream_index].outputCodec);
	height = transcoding_stream[stream_index].height;
	width = transcoding_stream[stream_index].width;

	// if (g_strcmp0(codec, "h264") && g_strcmp0(codec, "h265")&& g_strcmp0(codec, "h264-5")) {
	//  	goto fail;
	// }
	//Make video part
	//rtspsrc ! rtph264depay ! nvv4l2decoder ! nvv4l2h264enc ! rtph264pay
	if (input_codec.find("H.264") != string::npos && output_codec.find("H.264") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph264depay", fail, "depay");
		// MAKE_AND_ADD(parse,ret,"h264parse",fail,"parse");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h264enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph264pay", fail, NULL);

	} else if (input_codec.find("H.265") != string::npos && output_codec.find("H.265") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph265depay", fail, "depay");
		// MAKE_AND_ADD(parse,ret,"h265parse",fail,"parse");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h265enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph265pay", fail, NULL);

	} else if (input_codec.find("H.264") != string::npos && output_codec.find("H.265") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph264depay", fail, "depay");
		// MAKE_AND_ADD(parse,ret,"h265parse",fail,"parse");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h265enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph265pay", fail, NULL);

	} else if (input_codec.find("H.265") != string::npos && output_codec.find("H.264") != string::npos) {
		MAKE_AND_ADD(src, ret, "rtspsrc", fail, "rtspsrc");
		MAKE_AND_ADD(depay, ret, "rtph265depay", fail, "depay");
		// MAKE_AND_ADD(parse,ret,"h265parse",fail,"parse");
		MAKE_AND_ADD(dec, pbin, "nvv4l2decoder", fail, NULL);
		MAKE_AND_ADD(enc, pbin, "nvv4l2h264enc", fail, NULL);
		MAKE_AND_ADD(pay, pbin, "rtph264pay", fail, NULL);

	} else {
		goto fail;
	}

	g_signal_connect(src, "pad-added", G_CALLBACK(pad_added_cb), depay);

	streamUrl =transcoding_stream[stream_index].stream;
	bitrate = transcoding_stream[stream_index].bitrate;

	if (!streamUrl || bitrate  == 0) {
		g_print("Error: stream is NULL or bitrate is not setted\n");
	} else {
		g_print("Connect to %s, codec:%s, ", streamUrl, transcoding_stream[stream_index].inputCodec);
	}
	g_object_set(src, "location", streamUrl, NULL);
    g_object_set (G_OBJECT (src), "latency", 1000, NULL);
    g_object_set (G_OBJECT (src), "drop-on-latency", TRUE, NULL);
	
	gst_bin_add(GST_BIN(ret), pbin);

#ifdef __ENABLE_OPTIONS
	//Change params
	// if (qos){
	// 	g_object_set(enc, "qos", TRUE, NULL);
	// }
	if (bitrate == 1) {
		g_print("bitrate: %d Mb\n", BIT_RATE_PROFILE_1/1000000);
		g_object_set(enc, "bitrate", BIT_RATE_PROFILE_1, NULL);
	} else if (bitrate == 2) {
		g_print("bitrate: %d Mb\n", BIT_RATE_PROFILE_2/1000000);
		g_object_set(enc, "bitrate", BIT_RATE_PROFILE_2, NULL);
	} else if (bitrate == 3) {
		g_print("bitrate: %d Mb\n", BIT_RATE_PROFILE_3/1000000);
		g_object_set(enc, "bitrate", BIT_RATE_PROFILE_3, NULL);
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
#else
	//fixed mode
	g_object_set(enc, "bitrate", DEFAULT_BIT_RATE_PROFILE, NULL);
#endif

#ifdef __CHANGE_RESOLUION

	if (width > 0 && height > 0){
		printf("Set scale width: %d, height: %d\n", width, height);

		MAKE_AND_ADD(scale, pbin, "nvstreammux", fail, NULL);
		g_object_set(G_OBJECT(scale), "height", height , NULL);
		g_object_set(G_OBJECT(scale), "width", width, NULL);
		g_object_set(G_OBJECT(scale), "batch-size", 1, NULL);

		if(!link_element_to_streammux_sink_pad(scale,dec,0)){
			g_print("Link steammux false\n");
			goto fail;
		}	
		if (!gst_element_link_many(scale,enc,pay ,NULL)) {
			g_print("Link filter false\n");
			goto fail;
		}
	} else {
		if (!gst_element_link_many(dec, enc, pay , NULL)) {
			g_print("Link dec enc false\n");
			goto fail;
		}
	}
	if(!gst_element_link_many(depay,dec,NULL)){
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
	return ret;

fail:
	g_print("Error: Can not link to stream\n");
	gst_object_unref(ret);
	ret = NULL;
	goto done;
}

static void
onvif_factory_class_init(OnvifFactoryClass * klass)
{
	GstRTSPMediaFactoryClass *mf_class = GST_RTSP_MEDIA_FACTORY_CLASS(klass);
	//mf_class->create_element = onvif_factory_create_element;
	mf_class->create_element = rtsp_create_element;
}

static GstRTSPMediaFactory *
onvif_factory_new(void)
{
	GstRTSPMediaFactory *result;

	result =
		GST_RTSP_MEDIA_FACTORY(g_object_new(onvif_factory_get_type(), NULL));

	return result;
}

//Now do not need to use, it's past
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
}


static void on_client_new_session(GstRTSPClient * self,
	GstRTSPSession * object,
	gpointer user_data) {
	g_print("New session\n\n");
	gst_rtsp_session_set_timeout(object, 20);
}

//Handle when client connected
//Now do not need to use, it's past
static void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data)
{
	g_print("Client connected\n");
	g_signal_connect_object(client, "new-session", G_CALLBACK(on_client_new_session), NULL, G_CONNECT_AFTER);

	//g_signal_connect_object(client, "options-request", G_CALLBACK(on_request), NULL, G_CONNECT_AFTER);

	//GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);
	//GstRTSPUrl *url = gst_rtsp_connection_get_url(conn);
	//g_print("Abs path: %s\n", gst_rtsp_url_get_request_uri(url));
}



int
main(int argc, char *argv[])
{
	//putenv("GST_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/gstreamer-1.0/");	
	GMainLoop *loop;
	GstRTSPServer *server;
	GstRTSPMountPoints *mounts;
	GstRTSPMediaFactory *factory;
	GOptionContext *optctx;
	GError *error = NULL;
	gchar *service;
		GOptionEntry entries[] =
	{
		//{ "rtsp-port", 'p', 0, G_OPTION_ARG_STRING, &port, "RTSP server port", NULL },
		//{ "stream-1", 's', 0,  G_OPTION_ARG_STRING, &stream1, "rtsp stream to proxies 1", NULL },
		//{ "stream-2", 'l', 0,  G_OPTION_ARG_STRING, &stream2, "rtsp stream to proxies 2", NULL },		
		//{ "username", 'u', 0, G_OPTION_ARG_STRING, &username, "Username for authentication", NULL },
		//{ "password", 'w', 0, G_OPTION_ARG_STRING, &password, "Password for authentication", NULL },
		{ "config", 'c', 0,  G_OPTION_ARG_STRING, &stream_config_file, "config file for rtsp streaming", NULL },
#ifdef __ENABLE_OPTIONS
			//videoscale option
		#ifdef __CHANGE_RESOLUION
		//{ "width", 'e', 0, G_OPTION_ARG_INT, &width, "Resize: width", NULL },
		//{ "height", 'h', 0, G_OPTION_ARG_INT, &height, "Resize: height", NULL },
		#endif
			//x264enc option
		//{ "qos", 'q', 0, G_OPTION_ARG_INT, &qos, "encoder: QoS", NULL },
		//{ "profile", 'f', 0, G_OPTION_ARG_INT, &profile, "encoder: profile", NULL },
		//{ "bitrate-1", 0, 0, G_OPTION_ARG_INT, &bitrate1, "encoder: bitrate profile ( 1 or 2 ) of output stream", NULL },
		//{ "bitrate-2", 0, 0, G_OPTION_ARG_INT, &bitrate2, "encoder: bitrate profile ( 1 or 2 ) of output stream", NULL },
		//{ "rc-mode", 'c', 0, G_OPTION_ARG_INT, &rc_mode, "encoder: bitrate mode", NULL },
		//{ "iframeinterval", 'i', 0, G_OPTION_ARG_INT, &iframeinterval, "encoder: iframe interval", NULL },

		//End of nvh264enc
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
	if(stream_config_file==NULL){
		g_print("Config file not setted.\n");
		return 1;
	}
	// read in the json file
	ifstream f(stream_config_file, ifstream::in);

    json jsonData; //create unitiialized json object

    f >> jsonData; // initialize json object with what was read from file

    //cout << jsonData << endl; // prints json object to screen
	string str = jsonData.at("port").get<string>();
	vector<char> chars(str.c_str(), str.c_str() + str.size() + 1u);
	port=&chars[0];

	//cout<<port<<endl;
	

    // uses at to access fields from json object
	for(auto &arr: jsonData["streams"]){
		uri_str[stream_count] = arr.at("stream_uri").get<string>();
		if (!validate_url(uri_str[stream_count])) {
			cout << "URL '" << uri_str[stream_count] << "' is not of RTSP type, or wrong format!" << endl << endl;
			continue;
		}

		// Parse input codec
		input_codec_str[stream_count] = get_video_codec(uri_str[stream_count]);
		if (input_codec_str[stream_count] != NULL) {
			transcoding_stream[stream_count].inputCodec = input_codec_str[stream_count];
			cout << "Found video codec: " << input_codec_str[stream_count] << endl << endl;
		} else {
			g_printerr("Input video codec not found!\n\n");
			continue;
		}

		// Parse output codec
		output_codec_str[stream_count] = arr.at("output_codec").get<string>();
		if (input_codec_str[stream_count] != NULL) {
			transcoding_stream[stream_count].outputCodec = (gchar*)output_codec_str[stream_count].data();
		} else {
			g_printerr("Output video codec not found!\n\n");
			continue;
		}
		
		// Parse stream
		transcoding_stream[stream_count].stream = (gchar*)uri_str[stream_count].data();

		// Parse bitrate
		try {
			transcoding_stream[stream_count].bitrate = stoi(arr.at("bitrate").get<string>());
		} catch (const exception& e) {
			cout << e.what() << ". Using camera bitrate setting.\n";
			transcoding_stream[stream_count].bitrate = 0;
		}

		// Parse resolution
		try {
			transcoding_stream[stream_count].width = stoi(arr.at("width").get<string>());
		} catch (const exception& e) {
			cout << e.what() << ". Using camera resolution setting.\n";
			transcoding_stream[stream_count].width = 0;
		}

		try {
			transcoding_stream[stream_count].height = stoi(arr.at("height").get<string>());
		} catch (const exception& e) {
			cout << e.what() << ". Using camera resolution setting.\n";
			transcoding_stream[stream_count].height=0;
		}
		
		stream_count++;
	}

	if (stream_count <= 0) {
		g_printerr("NO INPUT STREAMS FOUND!\n");
		return 0;
	}

	for (int i=0; i < stream_count; i++){
		if (transcoding_stream[i].stream == NULL || transcoding_stream[i].outputCodec == NULL){
			g_printerr("Parsing setting file failed!\n");
			return 1;
		}
		cout << "URL: '" << transcoding_stream[i].stream 
				<< "', bitrate: " << transcoding_stream[i].bitrate 
				<< " Mbps, input codec: " << transcoding_stream[i].inputCodec 
				<< ", output codec: " << transcoding_stream[i].outputCodec 
				<< ", resolution: " << transcoding_stream[i].width << "x" << transcoding_stream[i].height 
				<< endl;
	}
	if(port==0){
		g_printerr("RTSP Port not setted.\n");
		return 1;
	}
	g_option_context_free(optctx);

	GST_DEBUG_CATEGORY_INIT(onvif_server_debug, "onvif-server", 0,
		"ONVIF server");

	loop = g_main_loop_new(NULL, FALSE);
	
	server = gst_rtsp_onvif_server_new();
	gst_rtsp_server_set_service(server, port);
	mounts = gst_rtsp_server_get_mount_points(server);

	//Create facory
	factory = onvif_factory_new();
	gst_rtsp_media_factory_set_media_gtype(factory, GST_TYPE_RTSP_ONVIF_MEDIA);
	gst_rtsp_server_set_address(server, "0.0.0.0");
	// if (username && password) {
	// 	makeAuth(factory, server);
	// }
	
	for(int i=1;i<=stream_count;i++){
		char init_uri[] = "/stream-";
		string s = to_string(i);
		char *index =(char*) s.c_str(); 
		//cout<<strcat(init_uri,index)<<endl;
		gst_rtsp_mount_points_add_factory(mounts, (const gchar*)strcat(init_uri,index), factory);
	}
	g_object_unref(mounts);
	g_signal_connect(server, "client-connected", G_CALLBACK(on_client_connected), NULL);
	gst_rtsp_server_attach(server, NULL);

	service = gst_rtsp_server_get_service(server);
	g_print("Stream ready at:\n");
	for (int i=1; i<=stream_count; i++) {
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