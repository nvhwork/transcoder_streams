#include <iostream>
#include <gst/gst.h>
#include "json.hpp"

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#define PATH_ID_LOWER_BOUND 10000000
#define PATH_ID_UPPER_BOUND 99999999

using json = nlohmann::json;
using namespace std;

int parse_streams_from_json (json jsonData);
void on_client_connected(GstRTSPServer * server, GstRTSPClient * client, gpointer user_data);
GstRTSPMediaFactory * onvif_factory_new(void);
