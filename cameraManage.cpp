#include "cameraManage.hpp"

using json = nlohmann::json;
using namespace std;

static gchar *stream_config_file = NULL;

json get_camera_from_input(int argc, char *argv[]) {
	GOptionContext *optctx;
	GError *error = NULL;
	GOptionEntry entries[] = {
		{ "config", 'c', 0,  G_OPTION_ARG_STRING, &stream_config_file, "Config file for RTSP streaming", NULL },
		{ NULL }
	};
	optctx = g_option_context_new("RTSP PROXY");
	g_option_context_add_main_entries(optctx, entries, NULL);
	g_option_context_add_group(optctx, gst_init_get_option_group());

	if (!g_option_context_parse(optctx, &argc, &argv, &error)) {
		g_printerr("Error parsing options: %s\n", error->message);
		g_option_context_free(optctx);
		g_clear_error(&error);
		return NULL;
	}

	g_option_context_free(optctx);
	if (stream_config_file == NULL){
		g_print("Config file not set.\n");
		return NULL;
	}

	// Read the JSON file
	ifstream f(stream_config_file, ifstream::in);
	json jsonData; // Create JSON object
	f >> jsonData; // Initialize JSON object with what was read from file

	return jsonData;
}

/**
 * @brief Parse camera information from JSON input
 * 
 * @param arr 
 * @return int 	 0 - Successful
 * 				-1 - Invalid URL type
 * 				-2 - URL does not contain video stream
 * 				-3 - Camera already exists in the system
 */
int add_camera(json arr) {
	/* Access fields from JSON object */
	int cameraWidth, cameraHeight;
	string cameraUrl, cameraName, cameraCodec;

	// Parse camera name and URL
	cameraName = arr.at("camera_name").get<string>();
	cameraUrl = arr.at("camera_url").get<string>();

	if (!validate_url(cameraUrl)) {
		cerr << "Camera URL is invalid!" << endl;
		return -1;
	}

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

	// Check whether the URL exists in the database
	res = stmt->executeQuery("SELECT camera_url FROM cameras;");
	while (res->next()) {
		string url = res->getString(1);
		if (url.compare(cameraUrl) == 0) {
			cerr << "Camera already exists in the system" << endl;
			delete res;
			delete stmt;
			delete conn;
			return -3;
		}
	}

	// Get camera information
	json camInfo = get_camera_info(cameraUrl);
	if (camInfo == NULL) {
		cerr << "URL does not contain video stream!" << endl;
		return -2;
	}
	cameraCodec = camInfo.at("codec").get<string>();

	try {
		cameraWidth = camInfo.at("width").get<int>();
	} catch (const exception& e) {
		cerr << e.what() << endl;
	}

	try {
		cameraHeight = camInfo.at("height").get<int>();
	} catch (const exception& e) {
		cerr << e.what() << endl;
	}

	// Add to database
	stmt->execute("INSERT INTO cameras VALUES ('" 
			+ cameraName + "', '" + cameraUrl + "', " + to_string(cameraWidth) 
			+ ", " + to_string(cameraHeight) + ", '" + cameraCodec + "');");

	delete res;
	delete stmt;
	delete conn;

	cout << "\n**********************\n" << "Complete adding camera!" << endl;
	return 0;

}

int delete_camera(string cameraName) {
	int returnRes;
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

	// Check whether the camera name exists in the database
	res = stmt->executeQuery("SELECT camera_name FROM cameras WHERE camera_name = '" + cameraName + "';");
	if (!res->next()) {
		cerr << "Camera not found" << endl;
		returnRes = -1;
	} else {
		// Delete the row from database
		stmt->execute("DELETE FROM cameras WHERE camera_name = '" + cameraName + "';");
		returnRes = 0;
	}

	delete res;
	delete stmt;
	delete conn;
	return returnRes;
}

int main(int argc, char *argv[]) {
	json input;
	int returnRes;

	/* Get input from JSON */
	input = get_camera_from_input(argc, argv);

	/* Add camera */
	returnRes = add_camera(input);

	return returnRes;
}