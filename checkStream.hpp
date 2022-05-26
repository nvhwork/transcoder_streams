#include <iostream>
#include <regex>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include "json.hpp"

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using json = nlohmann::json;
using namespace std;

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstDiscoverer *discoverer;
  GMainLoop *loop;
} CustomData;

bool validate_url (string url);
static void get_video_codec_in_tags (const GstTagList * tags);
static void on_discovered_cb (GstDiscoverer * discoverer, GstDiscovererInfo * info, GError * err, CustomData * data);
static void on_finished_cb (GstDiscoverer * discoverer, CustomData * data);
json get_camera_info (string url);