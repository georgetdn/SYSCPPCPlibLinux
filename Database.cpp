#include "Database.h"
#include "Record.h"

#pragma pack(push, 1)  // Aligns members on 1-byte boundaries
struct HEADER
{
	int RecSize;
	char RecName[REC_NAME_SIZE];
	long long int primaryKey;
};
#pragma pack(pop)  // Restores the previous packing alignment

// Constructor
Database::Database(std::string fileName)
{
	FileName = fileName;
	Connect(FileName);
}

// Destructor
Database::~Database(void) {
	if (outFile.is_open()) {
		outFile.close();
	}
}
const std::string  Database::GetDatabaseName(void)
{
	return  FileName;
}
// Method to connect to the file
std::fstream& Database::Connect(std::string outFileName)
{
	if (IsOpen())
		Close();

	// Open the file for reading and writing (not appending)
	outFile.open(outFileName, std::ios::in | std::ios::out | std::ios::binary);
	if (!outFile) {
		// If file does not exist, create it
		outFile.open(outFileName, std::ios::out | std::ios::binary);
		outFile.close();
		// Re-open the file for reading and writing
		outFile.open(outFileName, std::ios::in | std::ios::out | std::ios::binary);
	}
	Record::setDatabase(*this);
	FileName = outFileName;

	return outFile;
}
bool Database::IsOpen(void)
{
	return outFile.is_open();
}
int Database::Close(void)
{
	if (IsOpen())
	{
		outFile.close();
		return 0;
	}
	return 1;
}
long Database::GetCount(void)
{
	long cnt = 0;
	struct HEADER
	{
		int RecSize;
		char RecName[REC_NAME_SIZE];
		long long int primaryKey;
	} header;
	outFile.seekg(0, std::ios::beg);
	while (true)
	{
		outFile.read((char*)(&header), sizeof(HEADER));
		if (outFile.gcount() != sizeof(HEADER) || header.RecSize == 0)
		{
			outFile.clear();
			break;
		}

		outFile.seekg(header.RecSize - sizeof(HEADER), std::ios::cur);
		cnt++;
	}
	return cnt;
}
int Database::Dump(std::string recName)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database was not created in the application." << std::endl;
		return 1;
	}
	if (!IsOpen())
	{
		std::cout << "Database is not opened." << std::endl;
		return 1;

	}
	HEADER header;
	outFile.clear();
	outFile.seekg(0, std::ios::beg);
	long long int cnt = 0;
	while (true)
	{
		std::cout << std::endl;
		outFile.read((char*)(&header), sizeof(HEADER));
		if (outFile.gcount() != sizeof(HEADER) || header.RecSize == 0)
		{
			outFile.clear();
			break;
		}
		if (recName != header.RecName)
		{
			outFile.seekg(header.RecSize - sizeof(HEADER), std::ios::cur);
			continue;
		}
		std::streampos savedPosition = outFile.tellp();
		Record* rec = Record::GetRecordByIndex(header.primaryKey);
		if (rec)
			rec->Dump();
		delete rec;
		outFile.seekp(savedPosition);

		outFile.seekg(header.RecSize - sizeof(HEADER), std::ios::cur);
		cnt++;
	}
	std::cout << "Total number of records " << cnt << std::endl << std::endl;
	return 0;
}

int Database::Dump(void)
{
	if (Record::db == nullptr)
	{
		std::cout << "Database was not created in the application." << std::endl;
		return 1;
	}
	if (!IsOpen())
	{
		std::cout << "Database is not opened." << std::endl;
		return 1;

	}
	HEADER header;
	outFile.clear();
	outFile.seekg(0, std::ios::beg);
	long long int cnt = 0;
	while (true)
	{
		std::cout << std::endl;
		outFile.read((char*)(&header), sizeof(HEADER));
		if (outFile.gcount() != sizeof(HEADER) || header.RecSize == 0)
		{
			outFile.clear();
			break;
		}
		if (!header.primaryKey)
			continue;
		std::streampos savedPosition = outFile.tellp();
		Record* rec = Record::GetRecordByIndex(header.primaryKey);
		if (rec)
			rec->Dump();
		delete rec;
		outFile.seekp(savedPosition);

		outFile.seekg(header.RecSize - sizeof(HEADER), std::ios::cur);
		cnt++;
	}
	std::cout << "Total number of records " << cnt << std::endl << std::endl;
	return 0;
}

