#include <iostream>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

using namespace std;

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData {
  GstDiscoverer *discoverer;
  GMainLoop *loop;
} CustomData;

// gchar * codec = NULL;

bool validate_url (string url);
static void get_video_codec_in_tags (const GstTagList * tags);
static void on_discovered_cb (GstDiscoverer * discoverer, GstDiscovererInfo * info, GError * err, CustomData * data);
static void on_finished_cb (GstDiscoverer * discoverer, CustomData * data);
gchar * get_video_codec (string url);