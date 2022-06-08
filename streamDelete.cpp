#include "dataManage.hpp"

using json = nlohmann::json;
using namespace std;

int delete_stream(json arr) {
	int returnRes;
	string absPath = arr.at("stream_path").get<string>();
	
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
	res = stmt->executeQuery("SELECT stream_path FROM streams WHERE stream_path = '" + absPath + "';");
	if (!res->next()) {
		cerr << "Stream not found" << endl;
		returnRes = -1;
	} else {
		// Delete the row from database
		stmt->execute("DELETE FROM streams WHERE stream_path = '" + absPath + "';");
		returnRes = 0;
	}

	delete res;
	delete stmt;
	delete conn;
	return returnRes;
}

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		cerr << "You need to input JSON file!" << endl;
		return -4;
	}
	json input;
	string file = argv[1];
	int returnRes;

	/* Get input from JSON */
	input = get_data_from_input(file);

	/* Add camera */
	returnRes = delete_stream(input);

	return returnRes;
}