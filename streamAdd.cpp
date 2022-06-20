#include "dataManage.hpp"

using json = nlohmann::json;
using namespace std;

/**
 * @brief Parse camera information from JSON input
 * 
 * @param arr 
 * @return int 	 0 - Successful
 * 				-1 - Camera not found
 * 				-2 - Stream codec not found
 * 				-3 - Stream configuration already exists
 */
int add_stream(json arr) {
	/* Access fields from JSON object */
	string streamId, streamPath, streamInputCamera, streamCodec, streamWidth, streamHeight;
	int cameraWidth, cameraHeight;

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
	
	// Parse stream input camera name
	streamInputCamera = arr.at("camera_name").get<string>();
	res = stmt->executeQuery("SELECT camera_url FROM cameras WHERE camera_name = '" + streamInputCamera + "'");
	if (!res->next()) {
		cerr << "Camera not found" << endl;
		delete res;
		delete stmt;
		delete conn;
		return -1;
	}

	// Parse resolution
	try {
		streamWidth = arr.at("width").get<string>();
	} catch (const exception& e) {
		cout << e.what();
	}

	try {
		streamHeight = arr.at("height").get<string>();
	} catch (const exception& e) {
		cout << e.what();
	}

	if (stoi(streamHeight) < 100 || stoi(streamWidth) < 100) {
		cerr << "Resolution is too small" << endl;
		delete res;
		delete stmt;
		delete conn;
		return -1;
	}

	// Parse output codec
	streamCodec = arr.at("codec").get<string>();
	if (streamCodec.empty()) {
		g_printerr("Output video codec not found!\n\n");
		delete res;
		delete stmt;
		delete conn;
		return -2;
	}

	// Generate stream ID
	streamId = streamInputCamera + "_" + streamHeight;
	res = stmt->executeQuery("SELECT stream_id FROM streams WHERE stream_id = '" + streamId + "'");
	if (res->next()) {
		cerr << "Stream configuration has already existed." << endl;
		delete res;
		delete stmt;
		delete conn;
		return -3;
	}

	// Generate path
	streamPath = "/" + streamInputCamera + "/" + streamWidth + "x" + streamHeight;
	
	// Add to database
	stmt->execute("INSERT INTO streams(stream_path, stream_input_camera, stream_width, stream_height, stream_codec) VALUES ('" 
			+ streamPath + "', '" + streamInputCamera + "', " + streamWidth + ", " + streamHeight + ", '" + streamCodec + "');");
	
	cout << "\n**********************\n" << "Complete adding stream!" << endl;

	// Free MySQL variables
	delete res;
	delete stmt;
	delete conn;

	return 0;
}


int main(int argc, char *argv[]) {
	if (argc <= 1) {
		cerr << "You need to input JSON file" << endl;
		return -4;
	}
	json input;
	string file = argv[1];
	int returnRes;

	/* Get input from JSON */
	input = get_data_from_input(file);

	/* Add stream */
	returnRes = add_stream(input);

	return returnRes;
}