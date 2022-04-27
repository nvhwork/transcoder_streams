#include <iostream>
#include <regex>
#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include "validateUrl.hpp"

#define TIME_LIMIT_DISCOVER 30*GST_SECOND

using namespace std;

gchar * codec = NULL;

bool validate_url(string url) {
	if (url.empty()) {
		cout << "URL is empty!";
		return false;
	}

	regex url_regex("((rtsp|rtsps)://)([a-zA-Z0-9]{2,256}:[a-zA-Z0-9]{2,256}@)?[a-z0-9.]{7,256}(:[0-9]{1,4})?(/[a-zA-Z0-9-_]{1,256})?");
	if (!regex_match(url, url_regex)) {
		return false;
	}
	return true;
}

/* Get the string value of video codec */
static void get_video_codec_in_tags (const GstTagList * tags) {
	GValue val = { 0, };
	gchar *str;

	gst_tag_list_copy_value (&val, tags, "video-codec");
	if (!G_IS_VALUE (&val)) return;

	if (G_VALUE_HOLDS_STRING (&val))
		str = g_value_dup_string (&val);
	else
		str = gst_value_serialize (&val);

	// g_print ("\tVIDEO CODEC FOUND: %s\n", str);
	codec = g_strdup(str);
	g_free (str);

	g_value_unset (&val);
}

/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb (GstDiscoverer * discoverer, GstDiscovererInfo * info,
		GError * err, CustomData * data) {
	GstDiscovererResult result;
	const gchar *uri;
	const GstTagList *tags;
	GstDiscovererStreamInfo *sinfo;

	uri = gst_discoverer_info_get_uri (info);
	result = gst_discoverer_info_get_result (info);
	switch (result) {
		case GST_DISCOVERER_URI_INVALID:
			g_print ("Invalid URI '%s'\n", uri);
			break;
		case GST_DISCOVERER_ERROR:
			g_print ("Discoverer error: %s\n", err->message);
			break;
		case GST_DISCOVERER_TIMEOUT:
			g_print ("Timeout\n");
			break;
		case GST_DISCOVERER_BUSY:
			g_print ("Busy\n");
			break;
		case GST_DISCOVERER_MISSING_PLUGINS:
			const GstStructure *s;
			gchar *str;

			s = gst_discoverer_info_get_misc (info);
			str = gst_structure_to_string (s);

			g_print ("Missing plugins: %s\n", str);
			g_free (str);
			break;
		case GST_DISCOVERER_OK:
			g_print ("Discovered '%s'\n", uri);
			break;
	}

	if (result != GST_DISCOVERER_OK) {
		g_printerr ("This URI cannot be played.\n");
		codec = NULL;
		return;
	}

	tags = gst_discoverer_info_get_tags (info);
	get_video_codec_in_tags(tags);
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb (GstDiscoverer * discoverer, CustomData * data) {
  g_print ("Finished discovering\n");

  g_main_loop_quit (data->loop);
}

gchar * get_video_codec (string url) {
	CustomData data;
	GError *err = NULL;
	const gchar * uri = url.c_str();
	gchar * resultCodec;

	/* Initialize cumstom data structure */
	memset (&data, 0, sizeof (data));

	/* Initialize GStreamer */
	gst_init (NULL, NULL);

	g_print ("Discovering '%s'\n", uri);

	/* Instantiate the Discoverer */
	data.discoverer = gst_discoverer_new (TIME_LIMIT_DISCOVER, &err);
	if (!data.discoverer) {
		g_print ("Error creating discoverer instance: %s\n", err->message);
		g_clear_error (&err);
		return NULL;
	}

	/* Connect to the interesting signals */
	g_signal_connect (data.discoverer, "discovered",
		G_CALLBACK (on_discovered_cb), &data);
	g_signal_connect (data.discoverer, "finished", G_CALLBACK (on_finished_cb),
		&data);

	/* Start the discoverer process (nothing to do yet) */
	gst_discoverer_start (data.discoverer);

	/* Add a request to process asynchronously the URI passed through the command line */
	if (!gst_discoverer_discover_uri_async (data.discoverer, uri)) {
		g_print ("Failed to start discovering URI '%s'\n", uri);
		g_object_unref (data.discoverer);
		return NULL;
	}

	/* Create a GLib Main Loop and set it to run, so we can wait for the signals */
	data.loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (data.loop);

	/* Stop the discoverer process */
	gst_discoverer_stop (data.discoverer);

	/* Free resources */
	g_object_unref (data.discoverer);
	g_main_loop_unref (data.loop);

	/* Return result of video codec */
	resultCodec = g_strdup(codec);
	g_free(codec);

	return resultCodec;
}
