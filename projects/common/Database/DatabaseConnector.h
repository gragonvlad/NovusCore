/*
# MIT License

# Copyright(c) 2018-2019 NovusCore

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions :

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
*/
#pragma once

#include <string>
#include <functional>
#include <vector>

#include <amy/connector.hpp>
#include "../Utils/SharedPool.h"
#include "../Utils/ConcurrentQueue.h"
#include "../NovusTypes.h"
#include "PreparedStatement.h"

enum DATABASE_TYPE
{
	AUTHSERVER,
	CHARSERVER,
	WORLDSERVER,
    DBC,
	COUNT
};

class DatabaseConnector;

struct AsyncSQLJob
{
	AsyncSQLJob()
	{
		type = DATABASE_TYPE::AUTHSERVER;
		sql = "";
		func = nullptr;
	}

	DATABASE_TYPE type;
	std::string sql;
	std::function<void(amy::result_set&, DatabaseConnector&)> func;
};

class DatabaseConnector
{
public:
	// Initialization
    static void Setup(std::string hosts[], u16 ports[], std::string usernames[], std::string passwords[], std::string names[]);

	// Static Connector creation
	static bool Create(DATABASE_TYPE type, std::unique_ptr<DatabaseConnector>& out);
    static bool Borrow(DATABASE_TYPE type, std::shared_ptr<DatabaseConnector>& out);
    static void Borrow(DATABASE_TYPE type, std::function<void(std::shared_ptr<DatabaseConnector>& connector)> const& func);

	// Async main function
	static void AsyncSQLThreadMain();

	// Static Async functions
    static void QueryAsync(DATABASE_TYPE type, std::string sql, std::function<void(amy::result_set& results, DatabaseConnector& connector)> const& func);
    static void ExecuteAsync(DATABASE_TYPE type, std::string sql);

	static void QueryAsync(DATABASE_TYPE type, PreparedStatement statement, std::function<void(amy::result_set& results, DatabaseConnector& connector)> const& func) { QueryAsync(type, statement.Get(), func); }
	static void ExecuteAsync(DATABASE_TYPE type, PreparedStatement statement) { ExecuteAsync(type, statement.Get()); }
	

	bool Execute(std::string sql);
	inline bool Execute(PreparedStatement statement) { return Execute(statement.Get()); }
	inline void ExecuteAsync(std::string sql) { ExecuteAsync(_type, sql); }
	inline void ExecuteAsync(PreparedStatement statement) { ExecuteAsync(_type, statement.Get()); }

	bool Query(std::string sql, amy::result_set& results);
	inline bool Query(PreparedStatement statement, amy::result_set& results) { return Query(statement.Get(), results); }
	inline void QueryAsync(std::string sql, std::function<void(amy::result_set& results, DatabaseConnector& connector)> const& func) { QueryAsync(_type, sql, func); }
	inline void QueryAsync(PreparedStatement statement, std::function<void(amy::result_set& results, DatabaseConnector& connector)> const& func) { QueryAsync(_type, statement.Get(), func); }
	
	~DatabaseConnector();
private:
	DatabaseConnector(); // Constructor is private because we don't want to allow newing these, use Create to aquire a smartpointer.
	bool _Connect(DATABASE_TYPE type);

	DATABASE_TYPE _type;
	amy::connector* _connector;

	static std::vector<std::string>  _hosts;
    static std::vector<std::string>  _usernames;
    static std::vector<std::string>  _passwords;
    static std::vector<std::string>  _names;
    static std::vector<u16>          _ports;
    static bool                      _initialized;

	static SharedPool<DatabaseConnector> _connectorPools[DATABASE_TYPE::COUNT];
	static std::thread* _asyncThread;
	static ConcurrentQueue<AsyncSQLJob> _asyncJobQueue;
	
};