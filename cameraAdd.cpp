#include "dataManage.hpp"

using json = nlohmann::json;
using namespace std;

/**
 * @brief Parse camera information from JSON input
 * 
 * @param arr 
 * @return int 	 0 - Successful
 * 				-1 - Invalid URL type
 * 				-2 - URL does not contain video stream
 * 				-3 - Camera already exists in the system
 * 				-4 - No JSON file found
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
		cerr << "You need to input JSON file" << endl;
		return -4;
	}
	json input;
	string file = argv[1];
	int returnRes;

	/* Get input from JSON */
	input = get_data_from_input(file);

	/* Add camera */
	returnRes = add_camera(input);

	return returnRes;
}