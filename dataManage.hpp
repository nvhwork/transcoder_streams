#include <iostream>
#include <fstream>
#include <string>
#include <glib.h>
#include <gio/gnetworking.h>
#include <gst/gst.h>
#include "checkStream.hpp"

using json = nlohmann::json;
using namespace std;

json get_data_from_input(string jsonFile);