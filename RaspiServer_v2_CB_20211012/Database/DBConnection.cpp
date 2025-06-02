/*
 * DBConnection.cpp
 *
 *  Created on: 29.04.2019
 *      Author: pi
 */

#include <Database/DBConnection.hpp>
#include <nlohmann/json.hpp>
#include "Curl/curl.h"
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdlib.h>
#include <time.h>

using namespace std;

using json = nlohmann::json;

CURLcode res;
const std::string DBConnection::sensortable    = "sensorsafedb.db";

DBConnection& DBConnection::getInstance()
{
    static DBConnection A;
    return A;
}

DBConnection::DBConnection() {
	// TODO Auto-generated constructor stub
	std::ifstream db_istrm("DBconfig.json");
	json db;// = json::parse(db_istrm);

	db_istrm >> db;
	this->db_ip= db["db_ip"];//"localhost";
	this->db_port = db["db_port"];//"8086";
	this->db_name  = db["db_name"];//"TestNet";
	this->db_measurement = db["db_measurement"];//"Sensors";
	//this->shadowsize = 0.0;
	///this->numberOfcells = 0;
	//open database
	if(SQLITE_OK != sqlite3_open(DBConnection::sensortable.c_str(), &sensordb)){
		std::cout << "Fehler in SQLITE-Open im DBConn-Konstruktor";
		throw -1;
	}
	std::cout << "sqlite DB should be open" << std::endl;
	//create the table if not available
	sqlite3_stmt *stmt;
	if(SQLITE_OK != sqlite3_prepare_v2(sensordb, "CREATE TABLE IF NOT EXISTS `Sensors` ( `Sensorid`	TEXT NOT NULL, `Sensortyp`	TEXT NOT NULL,	`Sensordescription`	TEXT NOT NULL,	`Unit`	TEXT NOT NULL,	`Accuracy`	REAL NOT NULL);", -1, &stmt, NULL)){
		std::cout << "Fehler in SQLITE-Prepare im DBConn-Konstruktor";
		throw -1;
	}
	if(SQLITE_DONE != sqlite3_step(stmt)){
		std::cout << "Fehler in SQLITE-Step im DBConn-Konstruktor";
		throw -1;
	}
	if(SQLITE_OK != sqlite3_finalize(stmt)){
		std::cout << "Fehler in SQLITE-Finalize im DBConn-Konstruktor";
		throw -1;
	}


}

DBConnection::~DBConnection() {
    sqlite3_close(sensordb);
	// TODO Auto-generated destructor stub
}

std::string influxDBSpaces(std::string input){
	std::string ret = "";

	istringstream f(input);
	string s;
	bool first = true;
	while(std::getline(f, s, ' ')){
		if(!first) ret += "\\ " + s;
		else{
			ret += s;
			first = false;
		}
	}

	return ret;
}

void DBConnection::WriteDB(MSensorValue &message){
	std::lock_guard<std::mutex> lock(sensordblock);
	sqlite3_stmt *stmt;

	//Sensor Characteristics
	this->id  = message.getSensorID();
	this->sensortype  = message.getSensorType();
	this->sensordescription = message.getSensorDescription();
	this->unit = message.getUnit();
	this->accuracy = message.getAccuracy();

	//read in all sensors under SensorID
	std::string query = std::string("SELECT rowid, Sensordescription, Unit, Accuracy FROM Sensors WHERE Sensorid='") + message.getSensorID() + "' AND Sensortyp = '"+message.getSensorType()+"';";
	if(SQLITE_OK != sqlite3_prepare_v2(sensordb, query.c_str(), -1, &stmt, NULL)) std::cout << "Fehler in SQLITE-Prepare in -WriteDB- GETSENSORS";
	int rowid = 0;
	std::string description = "";
	std::string unit = "";
	float accu = -666;
	int ret = sqlite3_step(stmt);
	if(ret == SQLITE_ROW){
		if(ret == SQLITE_ROW) {
			rowid = sqlite3_column_int64(stmt, 0);

			const unsigned char *text = static_cast<const unsigned char*>(sqlite3_column_text(stmt, 1));
			unsigned int length = sqlite3_column_bytes(stmt, 1);
			description = (text == nullptr)? "" : std::string(reinterpret_cast<const char*>(text), length);

			text = static_cast<const unsigned char*>(sqlite3_column_text(stmt, 2));
			length = sqlite3_column_bytes(stmt, 2);
			unit = (text == nullptr)? "" : std::string(reinterpret_cast<const char*>(text), length);

			accu = sqlite3_column_double(stmt, 3);
			std::cout << "Readed data for this Sensor: " << rowid << "|" << description << "|" << unit << "|" << accu << std::endl;
		}
		else if(ret != SQLITE_DONE) std::cout << "Fehler in SQLITE-STEP in -WriteDB-";
	}
	else if(ret != SQLITE_DONE) std::cout << "Fehler in SQLITE-STEP in -WriteDB- GETSENSORS";
	if(SQLITE_OK != sqlite3_finalize(stmt)) std::cout << "Fehler in SQLITE-Finalize in -WriteDB- GETSENSORS";

	if(rowid == 0){								//sensor needs to be written into database
		query = std::string("INSERT INTO Sensors (Sensorid, Sensortyp, Sensordescription, Unit, Accuracy) VALUES ( '"+message.getSensorID()+"' , '"+message.getSensorType()+"' , '"+message.getSensorDescription()+"' , '"+message.getUnit()+"' , "+std::to_string(message.getAccuracy())+");");
		if(SQLITE_OK != sqlite3_prepare_v2(sensordb, query.c_str(), -1, &stmt, NULL)) std::cout << "Fehler in SQLITE-Prepare in -WriteDB- INSERT";
		ret = sqlite3_step(stmt);
		if(ret != SQLITE_DONE) std::cout << "Fehler in SQLITE-STEP in -WriteDB- INSERT";
		if(SQLITE_OK != sqlite3_finalize(stmt)) std::cout << "Fehler in SQLITE-Finalize in -WriteDB- INSERT";
		std::cout << "Sensor gets written to Database." << std::endl;
	}
	else{											//check sensor for newer information
		if(message.getSensorDescription() == ""){	//datasaving packet, but we can revive the information
			std::cout << "Datasaving packet detected, set missing information." << std::endl;
			this->sensordescription = description + " |LoRa-Packet";
			this->unit = unit;
			this->accuracy = accu;
		}
		else{										//check if new sensorinformation needs to be updated in database
			if(description != message.getSensorDescription() || unit != message.getUnit() || accu != message.getAccuracy()){
				std::cout << "Sensorinformation has changed, update Database." << std::endl;
				query = std::string("UPDATE Sensors SET Sensordescription = '"+message.getSensorDescription()+"', Unit = '"+message.getUnit()+"', Accuracy = "+std::to_string(message.getAccuracy())+" WHERE rowid = "+std::to_string(rowid)+";");
				if(SQLITE_OK != sqlite3_prepare_v2(sensordb, query.c_str(), -1, &stmt, NULL)) std::cout << "Fehler in SQLITE-Prepare in -WriteDB- UPDATE";
				ret = sqlite3_step(stmt);
				if(ret != SQLITE_DONE) std::cout << "Fehler in SQLITE-STEP in -WriteDB- UPDATE";
				if(SQLITE_OK != sqlite3_finalize(stmt)) std::cout << "Fehler in SQLITE-Finalize in -WriteDB- UPDATE";
			}
		}
	}



	//catch empty fields
	if(this->sensordescription == "") this->sensordescription = "-";
	if(this->unit == "") this->unit = "-";
	//accuracy -666 is meant as "Not Available". Problem is, to differentiate, a 0 as "the right Value" or "no Value"; there is no -0.

	this->sensordescription = influxDBSpaces(this->sensordescription);

/*
	string timestring = message.getTime();
	const char *timestamp_string = timestring.c_str();
	struct tm tm;
	strptime(timestamp_string,"%Y-%m-%dT%H:%M:%SZ",&tm);
	time_t t = timegm(&tm);


	cout << to_string(t) << endl;
*/
	//const int32_t epoch_offset = 7200;
	time_t localtime = message.getTime(); //+ epoch_offset;

	cout << "Seconds since 1970: " << to_string(localtime) << endl;

	//stringstream




	float value = message.getValue();



	CURL *curl;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if(curl){

		//read JSON file
		/*std::vector<string> out;
		std::ifstream istrm("sensors.json");

		////auto x = std::copy(std::istream_iterator<string>(istrm),std::istream_iterator<string>(),std::back_inserter(out));

		json j = json::parse(istrm); //(istrm);


		int obj_counter = j.count(id);

		if(obj_counter > 0){

			this->szsid = j[id]["szsid"];
			this->strang = j[id]["string"];//.get<string>();
			this->moduleno = j[id]["moduleno"];//.get<string>();
			this->modulemanufacturer = j[id]["modulemanufacturer"];//.get<string>();
			this->numberOfcells = j[id]["numberOfcells"];//.get<int>();
			this->celltype = j[id]["celltype"];//.get<string>();
			this->cellsize = j[id]["cellsize"];//.get<string>();
			this->cellnumber = j[id]["cellnumber"];//.get<string>();
			this->shadowsize = j[id]["shadowsize"];//.get<double>();
			this->locationname = j[id]["locationname"];
			this->latitude = j[id]["latitude"];
			this->longitude = j[id]["longitude"];

			char valueArray[50];
			gcvt(value,5,valueArray);


			char shadowArray[50];
			gcvt(shadowsize,4,shadowArray);

			char latitudeArray[50];
			gcvt(latitude,10,latitudeArray);

			char longitudeArray[50];
			gcvt(longitude,10,longitudeArray);

			string url = "http://" + db_ip + ":" + db_port + "/write?db=" + db_name;

			str = db_measurement + ",id=" + id + ",szsid=" + szsid +  ",sensortype=" + sensortype + ",string=" + strang + ",modulno=" + moduleno + ",modulemanufacturer=" + modulemanufacturer + ",numberOfcells=" + to_string(numberOfcells) + ",celltype=" + celltype + ",cellsize=" + cellsize + ",cellnumber=" + cellnumber + ",shadowsize=" + shadowArray + ",location=" + locationname + ",latitude=" + latitudeArray + ",longitude=" + longitudeArray +" value=" + valueArray + " " + to_string(localtime) + "000000000";

			curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl,CURLOPT_POSTFIELDS, str.c_str());
			curl_easy_perform(curl);

			if(res != CURLE_OK)
				fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));

			//always cleanup
			curl_easy_cleanup(curl);
			curl_global_cleanup();
		}
		else{
			cout << "Sensor not in sensors.json config file!" << endl;
		}*/

		char valueArray[50];
		gcvt(value,5,valueArray);


		//char shadowArray[50];
		//gcvt(shadowsize,4,shadowArray);

		//char latitudeArray[50];
		//gcvt(latitude,10,latitudeArray);

		//char longitudeArray[50];
		//gcvt(longitude,10,longitudeArray);

		char AccuracyArray[50];
		gcvt(accuracy,6,AccuracyArray);

		string url = "http://" + db_ip + ":" + db_port + "/write?db=" + db_name;

		string str;
		str = db_measurement + ",id='" + id +  "',sensortype='" + sensortype + "',sensordescription='" + sensordescription + "',unit='" + this->unit + "',accuracy=" + AccuracyArray +" value=" + valueArray + " " + to_string(localtime) + "000000000";

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl,CURLOPT_POSTFIELDS, str.c_str());
		curl_easy_perform(curl);

		if(res != CURLE_OK)
			fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));

		//always cleanup
		curl_easy_cleanup(curl);
		curl_global_cleanup();
	}



}

