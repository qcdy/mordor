// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include <iostream>

#include "mordor/config.h"
#include "mordor/pq.h"
#include "mordor/version.h"
#include "mordor/statistics.h"
#include "mordor/test/test.h"
#include "mordor/test/stdoutlistener.h"

using namespace Mordor;
using namespace Mordor::PQ;
using namespace Mordor::Test;

#ifdef WINDOWS
#include <direct.h>
#define chdir _chdir
#endif

std::string g_goodConnString;
std::string g_badConnString;

int main(int argc, const char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
            << " <connection string>"
            << std::endl;
        return 1;
    }
    g_goodConnString = argv[1];
    Config::loadFromEnvironment();
    std::string newDirectory = argv[0];
#ifdef WINDOWS
    newDirectory = newDirectory.substr(0, newDirectory.rfind('\\'));
#else
    newDirectory = newDirectory.substr(0, newDirectory.rfind('/'));
#endif
    chdir(newDirectory.c_str());

    StdoutListener listener;
    bool result;
    if (argc > 3) {
        result = runTests(testsForArguments(argc - 2, argv + 2), listener);
    } else {
        result = runTests(listener);
    }
    std::cout << Statistics::dump();
    return result ? 0 : 1;
}

void constantQuery(const std::string &queryName = std::string(), IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    PreparedStatement stmt = conn.prepare("SELECT 1, 'mordor'", queryName);
    Result result = stmt.execute();
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 2u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<int>(0, 0), 1);
    MORDOR_TEST_ASSERT_EQUAL(result.get<long long>(0, 0), 1);
    MORDOR_TEST_ASSERT_EQUAL(result.get<const char *>(0, 1), "mordor");
    MORDOR_TEST_ASSERT_EQUAL(result.get<std::string>(0, 1), "mordor");
}

MORDOR_UNITTEST(PQ, constantQueryBlocking)
{ constantQuery(); }
MORDOR_UNITTEST(PQ, constantQueryAsync)
{ IOManager ioManager; constantQuery(std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, constantQueryPreparedBlocking)
{ constantQuery("constant"); }
MORDOR_UNITTEST(PQ, constantQueryPreparedAsync)
{ IOManager ioManager; constantQuery("constant", &ioManager); }

MORDOR_UNITTEST(PQ, invalidConnStringBlocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage"), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnStringAsync)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, invalidConnString2Blocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage="), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnString2Async)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("garbage=", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, invalidConnString3Blocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("host=garbage"), ConnectionException);
}
MORDOR_UNITTEST(PQ, invalidConnString3Async)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn("host=garbage", &ioManager), ConnectionException);
}

MORDOR_UNITTEST(PQ, badConnStringBlocking)
{
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn(g_badConnString), ConnectionException);
}
MORDOR_UNITTEST(PQ, badConnStringAsync)
{
    IOManager ioManager;
    MORDOR_TEST_ASSERT_EXCEPTION(Connection conn(g_badConnString, &ioManager), ConnectionException);
}

void queryAfterDisconnect(IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);

    close(PQsocket(conn.conn()));
    MORDOR_TEST_ASSERT_EXCEPTION(conn.execute("SELECT 1"), ConnectionException);
    conn.reset();
    Result result = conn.execute("SELECT 1");
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<int>(0, 0), 1);
}

MORDOR_UNITTEST(PQ, queryAfterDisconnectBlocking)
{ queryAfterDisconnect(); }
MORDOR_UNITTEST(PQ, queryAfterDisconnectAsync)
{ IOManager ioManager; queryAfterDisconnect(&ioManager); }

void fillUsers(Connection &conn)
{
    conn.execute("CREATE TEMPORARY TABLE users (id INTEGER, name TEXT, height SMALLINT, awesome BOOLEAN, company TEXT, gender CHAR)");
    conn.execute("INSERT INTO users VALUES (1, 'cody', 72, true, 'Mozy', 'M')");
    conn.execute("INSERT INTO users VALUES (2, 'brian', 70, false, NULL, 'M')");
}

template <class ParamType, class ExpectedType>
void queryForParam(const std::string &query, ParamType param, size_t expectedCount,
    ExpectedType expected,
    const std::string &queryName = std::string(), IOManager *ioManager = NULL)
{
    Connection conn(g_goodConnString, ioManager);
    fillUsers(conn);
    PreparedStatement stmt = conn.prepare(query, queryName);
    Result result = stmt.execute(param);
    MORDOR_TEST_ASSERT_EQUAL(result.rows(), expectedCount);
    MORDOR_TEST_ASSERT_EQUAL(result.columns(), 1u);
    MORDOR_TEST_ASSERT_EQUAL(result.get<ExpectedType>(0, 0), expected);
}

MORDOR_UNITTEST(PQ, queryForIntBlocking)
{ queryForParam("SELECT name FROM users WHERE id=$1", 2, 1u, "brian"); }
MORDOR_UNITTEST(PQ, queryForIntAsync)
{ IOManager ioManager; queryForParam("SELECT name FROM users WHERE id=$1", 2, 1u, "brian", std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForIntPreparedBlocking)
{ queryForParam("SELECT name FROM users WHERE id=$1::integer", 2, 1u, "brian", "constant"); }
MORDOR_UNITTEST(PQ, queryForIntPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT name FROM users WHERE id=$1::integer", 2, 1u, "brian", "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForStringBlocking)
{ queryForParam("SELECT id FROM users WHERE name=$1", "brian", 1u, 2); }
MORDOR_UNITTEST(PQ, queryForStringAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE name=$1", "brian", 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForStringPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE name=$1::text", "brian", 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForStringPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE name=$1::text", "brian", 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForSmallIntBlocking)
{ queryForParam("SELECT id FROM users WHERE height=$1", (short)70, 1u, 2); }
MORDOR_UNITTEST(PQ, queryForSmallIntAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE height=$1", (short)70, 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForSmallIntPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE height=$1::smallint", (short)70, 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForSmallIntPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE height=$1::smallint", (short)70, 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForBooleanBlocking)
{ queryForParam("SELECT id FROM users WHERE awesome=$1", false, 1u, 2); }
MORDOR_UNITTEST(PQ, queryForBooleanAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE awesome=$1", false, 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForBooleanPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE awesome=$1::boolean", false, 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForBooleanPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE awesome=$1::boolean", false, 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForNullBlocking)
{ queryForParam("SELECT id FROM users WHERE company=$1", Null(), 1u, 2); }
MORDOR_UNITTEST(PQ, queryForNullAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE company=$1", Null(), 1u, 2, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForNullPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE company=$1::text", Null(), 1u, 2, "constant"); }
MORDOR_UNITTEST(PQ, queryForNullPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE company=$1::text", Null(), 1u, 2, "constant", &ioManager); }

MORDOR_UNITTEST(PQ, queryForCharBlocking)
{ queryForParam("SELECT id FROM users WHERE gender=$1", 'M', 2u, 1); }
MORDOR_UNITTEST(PQ, queryForCharAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE gender=$1", 'M', 2u, 1, std::string(), &ioManager); }
MORDOR_UNITTEST(PQ, queryForCharPreparedBlocking)
{ queryForParam("SELECT id FROM users WHERE gender=$1::CHAR", 'M', 2u, 1, "constant"); }
MORDOR_UNITTEST(PQ, queryForCharPreparedAsync)
{ IOManager ioManager; queryForParam("SELECT id FROM users WHERE gender=$1::CHAR", 'M', 2u, 1, "constant", &ioManager); }