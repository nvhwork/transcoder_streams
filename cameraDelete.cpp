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
	returnRes = delete_camera(input);

	return returnRes;
}