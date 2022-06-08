#include <iostream>
#include <string>
#include <glib.h>
#include <gst/gst.h>
#include "json.hpp"
#include "server0audio.hpp"
#include "checkStream.hpp"

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

using json = nlohmann::json;
using namespace std;

void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data);
GstRTSPMediaFactory * onvif_factory_new(void);
