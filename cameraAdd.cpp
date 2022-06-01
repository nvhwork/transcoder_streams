#include "cameraManage.hpp"

using json = nlohmann::json;
using namespace std;

json get_camera_from_input(string url) {
	// Get JSON data from server
	string getCmd = "http get " + url + "/cameras/1 --output temp/camera.json";
	system(getCmd.c_str());

	// Read the JSON file
	ifstream f("temp/camera.json", ifstream::in);
	json jsonData; // Create JSON object
	f >> jsonData; // Initialize JSON object with what was read from file

	// Delete JSON file and data at server side
	string deleteCmd = "http delete " + url + "/cameras/1";
	system(deleteCmd.c_str());
	system("rm temp/camera.json");

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
 * 				-4 - No URL found
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

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		cerr << "You need to input server IP and port!" << endl;
		return -4;
	}
	json input;
	string url = argv[1];
	int returnRes;

	/* Get input from JSON */
	input = get_camera_from_input(url);

	/* Add camera */
	returnRes = add_camera(input);

	return returnRes;
}