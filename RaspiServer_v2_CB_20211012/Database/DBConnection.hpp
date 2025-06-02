/*
 * DBConnection.hpp
 *
 *  Created on: 29.04.2019
 *      Author: pi
 */

#ifndef DATABASE_DBCONNECTION_HPP_
#define DATABASE_DBCONNECTION_HPP_

#include <string>
#include "Utility/sqlite3.h"
#include <mutex>
#include "Message/Type/MSensorValue.hpp"

class DBConnection {
public:
    static DBConnection& getInstance();
	DBConnection();
	virtual ~DBConnection();
	void WriteDB(MSensorValue &message);
protected:
	std::string db_ip, db_port,db_name, db_measurement;
	std::string id, szsid, sensortype, sensordescription, unit, strang, moduleno, modulemanufacturer, celltype, cellsize, cellnumber,locationname;
	int numberOfcells;
	float shadowsize, accuracy, latitude, longitude;
	//sqlite DB
	static const std::string sensortable;
	std::mutex sensordblock;
	sqlite3 *sensordb;
};

#endif /* DATABASE_DBCONNECTION_HPP_ */
