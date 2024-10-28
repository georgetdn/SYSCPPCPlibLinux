#include "Database.h"
#include "Record.h"
#include <cstdarg>  // For va_list, va_start, va_end
#include <vector>
#include <string>
#include <algorithm>
#include <cctype> // for std::tolower
#include <chrono>

// Static function to access the record factory map with lazy initialization
std::map<std::string, Record* (*)()>& Record::getRecordFactory() {
	// This ensures that the map is initialized the first time this function is called
	static std::map<std::string, Record* (*)()> recordFactory;
	return recordFactory;
}
Database* nullDb = nullptr;  // Temporary placeholder for initialization
Database* Record::db = nullDb;  // Will be properly initialized later
long long Record::PrIdx = 0LL;

struct HEADER
{
	int RecSize;
	char RecName[REC_NAME_SIZE];
	long long int primaryKey;
};

Record::Record()
{

	recordDBAddress = std::streampos(-1);  // record was not saved to the db. Is is updated when 
	// this record is insreted or retrieved from the database
}
Record::Record(const Record &other):
	recordDBAddress(other.recordDBAddress)
{
}
void Record::setDatabase(Database& dbm)
{
	Record::db = &dbm;
}
bool Record::IsSaved(void)
{
	if (recordDBAddress == std::streampos(-1))
		return false;
	else
		return true;
}
bool Record::IsDeleted(void)
{
	long long idx;
	if (idx = GetPrimaryKey())
	{
		//check if this record is still in the database
		if (GetRecordByIndex(idx))
			return false;
		else
			return true;
	}
	else
	{
		return true;
	}
}
bool Record::Insert(void)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database is not opened." << std::endl;
		return false;
	}
	if (!GetRecName())
	{
		std::cout << "Record name is invalid." << std::endl;
		return false;
	}
	long long int tmp;
	auto start = std::chrono::high_resolution_clock::now();
	tmp = start.time_since_epoch().count();
	tmp = (tmp - tmp / 1000000000000 * 1000000000000) / 100;
	SetPrimaryKey(tmp);
	// Save the current position to the record address
	db->outFile.seekg(0, std::ios::end);
	recordDBAddress = db->outFile.tellp();
	db->outFile.write(reinterpret_cast<char*>(GetDataAddress()), GetDataSize());
	db->outFile.flush();

	return true;
}
bool Record::Update(void)
{
	if (Record::db == nullptr || !db->IsOpen()) {
		std::cout << "Database is not opened." << std::endl;
		return false;
	}

	if (recordDBAddress == std::streampos(-1)) {
		std::cout << "This instance of the class was not saved to the database." << std::endl;
		return false;
	}
	if (IsDeleted()) {
		std::cout << "This record was deleted." << std::endl;
		return false;
	}

	if (!GetRecName()) {
		std::cout << "Record name is invalid." << std::endl;
		return false;
	}

	// Check the file size and ensure the record address is within bounds
	db->outFile.seekg(0, std::ios::end);
	std::streampos fileSize = db->outFile.tellp();

	if (recordDBAddress >= fileSize) {
		std::cerr << "Error: recordDBAddress is beyond the file size." << std::endl;
		return false;
	}

	// Seek to the previously saved record address and update
	db->outFile.seekg(recordDBAddress);
	if (db->outFile.fail()) {
		std::cerr << "Error: seekg() failed. Could not move to position " << recordDBAddress << std::endl;
		db->outFile.clear();  // Clear fail state
		return false;
	}

	// Write data to the file
	db->outFile.write(GetDataAddress(), GetDataSize());
	if (db->outFile.fail()) {
		std::cerr << "Error: write() failed. Could not write " << GetDataSize() << " bytes." << std::endl;
		return false;
	}

	// Flush the stream to ensure data is written to disk
	db->outFile.flush();
	if (db->outFile.fail()) {
		std::cerr << "Error: flush() failed." << std::endl;
		return false;
	}

	return true;
}

bool Record::Delete(void)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database is not opened." << std::endl;
		return false;
	}

	if (recordDBAddress == std::streampos(-1))
	{
		std::cout << "This record was not saved to the database." << std::endl;
		return false;
	}
	if (IsDeleted())
	{
		std::cout << "This record has already been deleted." << std::endl;
		return false;
	}
	void* dataAddress = GetDataAddress();

	// Adjust the address by sizeof(int)  bytes
	char* adjustedAddress = reinterpret_cast<char*>(dataAddress) + sizeof(int);

	// Calculate the number of bytes to set to zero
	std::size_t sizeToSet = GetDataSize() - sizeof(int);

	// Set the memory block to zero
	std::memset(adjustedAddress, 0, sizeToSet);

	// Write n null bytes to the file
	std::vector<char> nullBytes(GetDataSize() - sizeof(int), '\0'); // Create a vector with 'n' null bytes
	db->outFile.seekg(recordDBAddress + static_cast<std::streamoff>(sizeof(int)));
	db->outFile.write(nullBytes.data(), GetDataSize() - sizeof(int));   // Write the entire buffer to the file
	db->outFile.flush();

	//recordDBAddress = std::streampos(-1);

	return true;
}
OpResult  Record::GetRecordByName(void)
{
	OpResult ret = OpResult::False;
	HEADER header;
	char* buffer = NULL;
	std::uint32_t  bufferSize = 0;

	while (true)
	{
		recordDBAddress = db->outFile.tellp();
		db->outFile.read((char*)(&header), sizeof(HEADER));

		if (db->outFile.gcount() != sizeof(HEADER) || header.RecSize == 0)
		{
			db->outFile.clear();
			ret = OpResult::False;
			break;
		}
		if (strcmp(header.RecName, GetRecName()) == 0)
		{
			buffer = new char[header.RecSize];
			bufferSize = header.RecSize;

			db->outFile.read(buffer, header.RecSize - sizeof(HEADER));
			if (db->outFile.gcount() != header.RecSize - sizeof(HEADER))
			{
				delete[] buffer;
				return OpResult::False;
			}
			memcpy((void*)(GetDataAddress()), &header, sizeof(HEADER));
			memcpy((void*)(GetDataAddress() + sizeof(HEADER)), buffer, header.RecSize - sizeof(HEADER));
			ret = OpResult::True;
			break;
		}
		else
		{
			db->outFile.seekp(header.RecSize - sizeof(HEADER), std::ios::cur);
			continue;
		}

	}



	return ret;
}
OpResult Record::Seek(recKey* k1, ...)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database is not opened." << std::endl;
		return OpResult::Null;
	}

	if (!db->IsOpen())
	{
		std::cout << "Database is not opened." << std::endl;
		return OpResult::Null;
	}
	// Start processing variable arguments
	va_list args;
	va_start(args, k1); // Initialize args to store all values after k2

	db->outFile.seekg(0, std::ios::beg);

	if (!k1)
		return GetRecordByName();


	char buff[sizeof(int) + REC_NAME_SIZE];
	char recName[REC_NAME_SIZE];
	char* buffer = NULL;
	std::uint32_t  bufferSize = 0;
	int recSz = 0;
	db->outFile.seekg(0, std::ios::beg);
	while (true)
	{
		LastOpResult = OpResult::Null;
		LastAndOr = AndOr::Null;

		db->outFile.read(buff, sizeof(int) + REC_NAME_SIZE);
		if (db->outFile.gcount() != sizeof(int) + REC_NAME_SIZE)
		{
			db->outFile.clear();
			return OpResult::False;
		}

		std::memcpy(&recSz, buff, sizeof(recSz));
		std::memcpy(recName, buff + sizeof(int), REC_NAME_SIZE);
		if (strcmp(recName, GetRecName()) != 0)
		{
			db->outFile.seekg(recSz - sizeof(int) - REC_NAME_SIZE, std::ios::cur);
			continue;
		}
		if (bufferSize < recSz)
		{
			delete[] buffer;
			buffer = new char[recSz];
			bufferSize = recSz;
		}
		db->outFile.read(buffer, recSz - sizeof(int) - REC_NAME_SIZE);
		if (db->outFile.gcount() != recSz - sizeof(int) - REC_NAME_SIZE)
		{
			delete[] buffer;
			return OpResult::False;
		}

		va_list args;
		va_start(args, k1); // Initialize args to store all values after k2
		// Example processing of the first two fixed arguments
		if (k1)
		{
			try {
				LastOpResult = processSeek(k1, buffer);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid argument: " << e.what() << std::endl;
				delete[] buffer;
				return OpResult::Null;
			}
			catch (const std::out_of_range& e) {
				std::cerr << "Out of range: " << e.what() << std::endl;
				delete[] buffer;
				return OpResult::Null;
			}
		}
		else
		{
			std::cout << "Invalid search." << std::endl;
			delete[] buffer;
			return OpResult::Null;;
		}

		// Example processing of additional arguments
		recKey* key = nullptr;
		// Process additional arguments
		while (true) {
			key = va_arg(args, recKey*);
			if (key == nullptr) {
				break;  // Exit the loop if no more arguments
			}

			try {
				LastOpResult = processSeek(key, buffer);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid argument: " << e.what() << std::endl;
				delete[] buffer;
				va_end(args);  // Clean up the argument list
				return OpResult::Null;
			}
			catch (const std::out_of_range& e) {
				std::cerr << "Out of range: " << e.what() << std::endl;
				delete[] buffer;
				va_end(args);  // Clean up the argument list
				return OpResult::Null;
			}
		}

		va_end(args);  // Clean up the argument list after processing all arguments

		if (LastOpResult == OpResult::True)
		{
			memcpy((void*)(GetDataAddress() + sizeof(int) + REC_NAME_SIZE), buffer, recSz - sizeof(int) - REC_NAME_SIZE);

			db->outFile.seekg(-recSz, std::ios::cur);
			recordDBAddress = db->outFile.tellp();
			db->outFile.seekg(recSz, std::ios::cur);

			delete[] buffer;
			return LastOpResult;
		}
	}
	delete[] buffer;
	return LastOpResult;
}

OpResult Record::Next(recKey* k1, ...)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database is not opened." << std::endl;
		return OpResult::Null;
	}

	if (!db->IsOpen())
	{
		std::cout << "Database is not opened." << std::endl;
		return OpResult::Null;
	}
	// Start processing variable arguments
	va_list args;
	va_start(args, k1); // Initialize args to store all values after k2

	if (!k1)
		return GetRecordByName();


	char buff[sizeof(int) + REC_NAME_SIZE];
	char recName[REC_NAME_SIZE];
	char* buffer = NULL;
	std::uint32_t  bufferSize = 0;
	int recSz = 0;
	while (true)
	{
		LastOpResult = OpResult::Null;
		LastAndOr = AndOr::Null;

		db->outFile.read(buff, sizeof(int) + REC_NAME_SIZE);
		if (db->outFile.gcount() != sizeof(int) + REC_NAME_SIZE)
		{
			db->outFile.clear();
			return OpResult::False;
		}

		std::memcpy(&recSz, buff, sizeof(recSz));
		std::memcpy(recName, buff + sizeof(int), REC_NAME_SIZE);
		if (strcmp(recName, GetRecName()) != 0)
		{
			db->outFile.seekg(recSz - sizeof(int) - REC_NAME_SIZE, std::ios::cur);
			continue;
		}
		if (bufferSize < recSz)
		{
			delete[] buffer;
			buffer = new char[recSz];
			bufferSize = recSz;
		}
		db->outFile.read(buffer, recSz - sizeof(int) - REC_NAME_SIZE);
		if (db->outFile.gcount() != recSz - sizeof(int) - REC_NAME_SIZE)
		{
			delete[] buffer;
			return OpResult::False;
		}

		va_list args;
		va_start(args, k1); // Initialize args to store all values after k2
		// Example processing of the first two fixed arguments
		if (k1)
		{
			try {
				LastOpResult = processSeek(k1, buffer);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid argument: " << e.what() << std::endl;
				delete[] buffer;
				return OpResult::Null;
			}
			catch (const std::out_of_range& e) {
				std::cerr << "Out of range: " << e.what() << std::endl;
				delete[] buffer;
				return OpResult::Null;
			}
		}
		else
		{
			std::cout << "Invalid search." << std::endl;
			delete[] buffer;
			return OpResult::Null;;
		}

		// Example processing of additional arguments
		recKey* key = nullptr;
		// Process additional arguments
		while (true) {
			key = va_arg(args, recKey*);
			if (key == nullptr) {
				break;  // Exit the loop if no more arguments
			}

			try {
				LastOpResult = processSeek(key, buffer);
			}
			catch (const std::invalid_argument& e) {
				std::cerr << "Invalid argument: " << e.what() << std::endl;
				delete[] buffer;
				va_end(args);  // Clean up the argument list
				return OpResult::Null;
			}
			catch (const std::out_of_range& e) {
				std::cerr << "Out of range: " << e.what() << std::endl;
				delete[] buffer;
				va_end(args);  // Clean up the argument list
				return OpResult::Null;
			}
		}

		va_end(args);  // Clean up the argument list after processing all arguments

		if (LastOpResult == OpResult::True)
		{
			memcpy((void*)(GetDataAddress() + sizeof(int) + REC_NAME_SIZE), buffer, recSz - sizeof(int) - REC_NAME_SIZE);

			db->outFile.seekg(-recSz, std::ios::cur);
			recordDBAddress = db->outFile.tellp();
			db->outFile.seekg(recSz, std::ios::cur);

			delete[] buffer;
			return LastOpResult;
		}
	}
	delete[] buffer;
	return LastOpResult;
}
OpResult Record::processSeek(recKey* k, const  char* buff)
{
	if (LastOpResult != OpResult::Null)
	{
		if (LastOpResult == OpResult::True)
		{
			if (LastAndOr == AndOr::Or)
			{
				LastAndOr = k->andOr;
				return OpResult::True;
			}
		}
		else
		{
			if (LastAndOr == AndOr::And)
			{
				LastAndOr = k->andOr;
				return OpResult::False;
			}
		}
	}

	//==============================
	if (k->typeInfo == typeid(bool))
	{

		// Convert to lowercase
		std::transform(k->value.begin(), k->value.end(), k->value.begin(),
			[](unsigned char c) { return std::tolower(c); });
		switch (k->comp)
		{

		case Comp::Equal:
			if (((k->value == "true" || k->value == "1") and (buff + k->offset)[0] == 1) ||
				((k->value == "false" || k->value == "0") and (buff + k->offset)[0] == 0))
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (((k->value == "true" || k->value == "1") and (buff + k->offset)[0] == 1) ||
				((k->value == "false" || k->value == "0") and (buff + k->offset)[0] == 0))
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		default:
			std::cout << "'Greater than' and 'Smaller than' operators does not apply to bool type." << std::endl;
		}
	}
	else if (k->typeInfo == typeid(char) ||
		k->typeInfo == typeid(signed char) ||
		k->typeInfo == typeid(unsigned char))
	{
		switch (k->comp)
		{
		case Comp::Equal:
			if (k->value.c_str()[0] == (buff + k->offset)[0])
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (k->value.c_str()[0] == (buff + k->offset)[0])
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		case Comp::Greater:
			if (k->value.c_str()[0] > (buff + k->offset)[0])
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::Smaller:
			if (k->value.c_str()[0] < (buff + k->offset)[0])
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::GreaterEq:
			if (k->value.c_str()[0] > (buff + k->offset)[0] ||
				k->value.c_str()[0] == (buff + k->offset)[0])
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::SmallerEq:
			if (k->value.c_str()[0] < (buff + k->offset)[0] ||
				k->value.c_str()[0] == (buff + k->offset)[0])
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		default:
			std::cout << "Invalid operator." << std::endl;
		}
	}
	else if (k->typeInfo == typeid(signed short int) ||
		k->typeInfo == typeid(signed  int) ||
		k->typeInfo == typeid(signed long int) ||
		k->typeInfo == typeid(signed long long int) ||
		k->typeInfo == typeid(unsigned short int) ||
		k->typeInfo == typeid(unsigned  int) ||
		k->typeInfo == typeid(unsigned long int))
	{
		signed long long int key;
		signed long long int val;
		if (k->typeInfo == typeid(signed short int))
		{
			signed short int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;
		}
		else if (k->typeInfo == typeid(signed  int))
		{
			signed  int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;
		}
		else if (k->typeInfo == typeid(signed long int))
		{
			signed long int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;

		}
		else if (k->typeInfo == typeid(signed long long int))
		{
			memcpy(&val, buff + k->offset, k->sz);
		}
		else if (k->typeInfo == typeid(unsigned short int))
		{
			unsigned short int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;

		}
		else if (k->typeInfo == typeid(unsigned  int))
		{
			unsigned  int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;

		}
		else
		{
			unsigned long int tmp;
			memcpy(&tmp, buff + k->offset, k->sz);
			val = tmp;

		}


		try {
			key = std::stoll(k->value);
		}
		catch (...) { // Catch any exception
			throw;  // Rethrow the exception to be handled by higher-level code
		}

		switch (k->comp)
		{
		case Comp::Equal:
			if (val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (val == key)
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		case Comp::Greater:
			if (val > key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::Smaller:
			if (val < key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::GreaterEq:
			if (val > key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::SmallerEq:
			if (val < key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		default:
			std::cout << "Invalid operator." << std::endl;
		}
	}
	else if (k->typeInfo == typeid(unsigned long long int))
	{
		unsigned long long int key;
		unsigned long long int val;

		std::cout << k->typeInfo.name() << std::endl;
		std::cout << k->sz << std::endl;

		memcpy(&val, buff + k->offset, k->sz);

		try {
			key = std::stoll(k->value);
		}
		catch (...) { // Catch any exception
			throw;  // Rethrow the exception to be handled by higher-level code
		}

		switch (k->comp)
		{
		case Comp::Equal:
			if (val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (val == key)
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		case Comp::Greater:
			if (val > key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::Smaller:
			if (val < key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::GreaterEq:
			if (val > key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::SmallerEq:
			if (val < key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		default:
			std::cout << "Invalid operator." << std::endl;
		}
	}
	else if (k->typeInfo.name()[0] == 'c' &&
		k->typeInfo.name()[1] == 'h' &&
		k->typeInfo.name()[2] == 'a' &&
		k->typeInfo.name()[3] == 'r' &&
		k->typeInfo.name()[5] == '[')
	{
		char* v = new char[k->sz];

		memcpy(v, buff + k->offset, k->sz);
		std::string val(v);
		std::string key = k->value;

		while (val.find(" ") != std::string::npos)
			val.replace(val.find(" "), 1, "");
		while (key.find(" ") != std::string::npos)
			key.replace(key.find(" "), 1, "");

		switch (k->comp)
		{
		case Comp::Equal:
			if (val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (val == key)
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		case Comp::Greater:
			if (val > key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::Smaller:
			if (val < key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::GreaterEq:
			if (val > key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::SmallerEq:
			if (val < key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		default:
			std::cout << "Invalid operator." << std::endl;
		}


	}
	else if (k->typeInfo.name()[0] == 'e' &&
		k->typeInfo.name()[1] == 'n' &&
		k->typeInfo.name()[2] == 'u' &&
		k->typeInfo.name()[3] == 'm')
	{

		std::string tmp = k->typeInfo.name();
		tmp.replace(tmp.find("enum "), 5, "");
		tmp += "::" + k->value;
		unsigned int key = GetEnumValue(tmp);
		if (key == -1)
			return LastOpResult;
		unsigned  int val;

		memcpy(&val, buff + k->offset, k->sz);

		switch (k->comp)
		{
		case Comp::Equal:
			if (val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::NotEqual:
			if (val == key)
				LastOpResult = OpResult::False;
			else
				LastOpResult = OpResult::True;
			break;
		case Comp::Greater:
			if (val > key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::Smaller:
			if (val < key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;
		case Comp::GreaterEq:
			if (val > key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		case Comp::SmallerEq:
			if (val < key || val == key)
				LastOpResult = OpResult::True;
			else
				LastOpResult = OpResult::False;
			break;

		default:
			std::cout << "Invalid operator." << std::endl;
		}
	}

	else
	{
		std::cout << "'" << k->typeInfo.name() << "' is not supported." << std::endl;
	}
	LastAndOr = k->andOr;
	return LastOpResult;
}
Record* Record::GetRecordByIndex(long long prIdx) {
	Record* newRecord = nullptr;

	std::string className = GetRecordName(prIdx);
	if (className.empty())
	{
		return nullptr;
	}
	PrIdx = prIdx;
	// Use the getRecordFactory() function to access the map
	if (getRecordFactory().find(className) != getRecordFactory().end())
	{
		newRecord = getRecordFactory()[className]();
	}

	return newRecord;
}
std::string Record::GetRecordName(long long prIdx)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database was not created in the application." << std::endl;
		return nullptr;
	}
	if (!db->IsOpen())
	{
		std::cout << "Database is not opened." << std::endl;
		return nullptr;

	}
	HEADER header;
	db->outFile.seekg(0, std::ios::beg);
	while (true)
	{
		db->outFile.read((char*)(&header), sizeof(HEADER));
		if (db->outFile.gcount() != sizeof(HEADER) || header.RecSize == 0)
		{
			db->outFile.clear();
			return "";
		}
		if (header.primaryKey == prIdx)
		{
			return header.RecName;
		}
		db->outFile.seekg(header.RecSize - sizeof(HEADER), std::ios::cur);

	}
	return "";
}
