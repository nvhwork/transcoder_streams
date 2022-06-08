#include "dataManage.hpp"

using json = nlohmann::json;
using namespace std;

json get_data_from_input(string jsonFile) {
	// Get JSON data from server
	// string getCmd = "http get " + url + "/cameras/1 --output temp/camera.json";
	// system(getCmd.c_str());

	// Read the JSON file
	ifstream f(jsonFile.c_str(), ifstream::in);
	json jsonData; // Create JSON object
	f >> jsonData; // Initialize JSON object with what was read from file

	// Delete JSON file and data at server side
	// string deleteCmd = "http delete " + url + "/cameras/1";
	// system(deleteCmd.c_str());
	// string deleteFileCmd = "rm " + jsonFile;
	// system(deleteFileCmd.c_str());

	return jsonData;
}