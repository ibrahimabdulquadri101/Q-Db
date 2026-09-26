#include "Catalog.h"
#include "DiskManager.h"
#include "Executor.h"
#include "WAL.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::string dbDir()
{
	const char *home = std::getenv("HOME");
	std::string qdbDir = home ? std::string(home) + "/.q-db" : ".q-db";
	try
	{
		if (std::filesystem::exists(qdbDir + "/data.bin"))
		{
			return qdbDir;
		}
		if (std::filesystem::exists("data") && std::filesystem::is_directory("data"))
		{
			return "data";
		}
		std::filesystem::create_directories(qdbDir);
		return qdbDir;
	}
	catch (...)
	{
		if (std::filesystem::exists("data") && std::filesystem::is_directory("data"))
		{
			return "data";
		}
		return ".";
	}
}

static void seedUsers(Executor &executor)
{
	// Names cycling through 10 options so the table has varied data
	static const char *names[] = {
		"Alice", "Bob", "Carol", "Dave", "Eve",
		"Frank", "Grace", "Hank", "Iris", "Jake"};

	std::cout << "Seeding 100 users...\n";
	for (int i = 1; i <= 100; ++i)
	{
		const std::string name = names[(i - 1) % 10];
		const int age = 20 + (i % 60); // ages 20-79
		const std::string sql =
			"INSERT INTO users VALUES (" +
			std::to_string(i) + ", '" + name + "', " +
			std::to_string(age) + ")";
		executor.execute(sql);
	}
	std::cout << "Done.\n";
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
	const std::string dir = dbDir();
	const std::string dbPath = dir + "/data.bin";
	const std::string walPath = dbPath + ".wal";

	DiskManager disk;
	if (!disk.open(dbPath))
	{
		std::cerr << "Fatal: could not open database at " << dbPath << '\n';
		return 1;
	}

	WAL wal;
	wal.init(walPath);
	wal.replay(disk);
	disk.setWAL(&wal);

	Catalog catalog;
	catalog.init(&disk);

	Executor executor(&catalog, &disk);

	// ------------------------------------------------------------------
	// Bootstrap: create the 'users' table and seed 100 rows on first run
	// ------------------------------------------------------------------
	if (!catalog.tableExists("users"))
	{
		executor.execute("CREATE TABLE users (id INT, name CHAR, age INT)");
		seedUsers(executor);
	}
	else
	{
		// Ensure user with ID 1 exists so the table always starts from 1
		Table *usersTable = catalog.getTable("users");
		if (usersTable != nullptr)
		{
			bool hasId1 = false;
			usersTable->scan([&](const Row &r) {
				if (r.id == 1) hasId1 = true;
			});
			if (!hasId1)
			{
				executor.execute("INSERT INTO users VALUES (1, 'Alice', 21)");
			}
		}
	}

	// ------------------------------------------------------------------
	// REPL
	// ------------------------------------------------------------------
	std::cout << "\nQ-Db interactive shell  (type 'exit' or 'quit' to stop)\n";
	std::cout << "Database: " << dbPath << "\n\n";

	std::string line;
	while (true)
	{
		std::cout << "q-db> ";
		std::cout.flush();

		if (!std::getline(std::cin, line))
		{
			// EOF (Ctrl-D)
			std::cout << '\n';
			break;
		}

		// Trim leading/trailing whitespace
		const auto first = line.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			continue;
		}
		line = line.substr(first);
		const auto last = line.find_last_not_of(" \t\r\n");
		if (last != std::string::npos)
		{
			line = line.substr(0, last + 1);
		}

		// Strip trailing semicolon if present
		if (!line.empty() && line.back() == ';')
		{
			line.pop_back();
		}

		if (line.empty())
		{
			continue;
		}

		// Exit commands
		std::string lower = line;
		for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		if (lower == "exit" || lower == "quit")
		{
			break;
		}

		executor.execute(line);
	}

	// Persist everything cleanly
	if (!disk.sync())
	{
		std::cerr << "Warning: database sync failed\n";
	}
	wal.checkpoint();
	disk.close();

	std::cout << "Goodbye.\n";
	return 0;
}
