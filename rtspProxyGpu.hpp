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
#define MAKE_AND_ADD(var, pipe, name, label, elem_name)

#define __CHANGE_RESOLUION
#define DEFAULT_INT
#define PROF_BASELINE
#define PROF_MAIN
#define PROF_HIGH
#define VIDENC_VARIABLE_BITRATE
#define VIDENC_CONSTANT_BITRATE
#define IFRAME_MAX
#define IFRAME_MIN

#define FIXED_BIT_RATE
#define BIT_RATE_PROFILE_1
#define BIT_RATE_PROFILE_2
#define BIT_RATE_PROFILE_3
#define DEFAULT_BIT_RATE_PROFILE
#define MAX_STREAM

static int check_rc(int rc);
static int check_iframe(int iframe);
static int check_profile(int prof);

static void enc_pad_added(GstElement * x264enc, GstPad * pad, GstGhostPad * ghost);
void pad_added_cb(GstElement * src, GstPad * srcpad, GstElement * peer);
void makeAuth(GstRTSPMediaFactory *factory, GstRTSPServer *server);

G_DEFINE_TYPE(OnvifFactory, onvif_factory, GST_TYPE_RTSP_MEDIA_FACTORY);
static void onvif_factory_init(OnvifFactory * factory);
gboolean link_element_to_streammux_sink_pad (GstElement *streammux, GstElement *elem, gint index);

static GstElement * rtsp_create_element(GstRTSPMediaFactory * factory, const GstRTSPUrl * url)
static void onvif_factory_class_init(OnvifFactoryClass * klass);
static GstRTSPMediaFactory * onvif_factory_new(void);

static void on_request(GstRTSPClient *client, GstRTSPContext *ctx, GstRTSPServer *server, gpointer p_user);
static void on_client_new_session(GstRTSPClient * self, GstRTSPSession * object, gpointer user_data);
static void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data);
