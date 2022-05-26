#include <iostream>
#include <fstream>
#include <string>
#include <glib.h>
#include <gio/gnetworking.h>
#include <gst/gst.h>
#include "checkStream.hpp"

using json = nlohmann::json;
using namespace std;

json get_camera_from_input(int argc, char *argv[]);
int add_camera(json arr);
int delete_camera(string cameraName);