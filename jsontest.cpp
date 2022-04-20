#include <iostream>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

int main(int argc, char const *argv[])
{
    // read in the json file
	std::ifstream f("test.json", std::ifstream::in);

    json j; //create unitiialized json object

    f >> j; // initialize json object with what was read from file

    std::cout << j << std::endl; // prints json object to screen

    // uses at to access fields from json object
    std::cout << j.at("cal") << std::endl;
    std::cout << j.at("months") << std::endl;

    int days = j.at("days");

    std::cout << days <<std::endl;

	return 0;
}