#include "../implementation.h"

#define BOOST_TEST_MAIN test
#define BOOST_TEST_MODULE implementationTest

#include <boost/test/included/unit_test.hpp>
#include <string>
#include <unordered_map>

float test1(int num) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    return 1.5;
}
float test2(int num) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    return 2.5;
}
float test3(int num) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    return 3.5;
}
float test4(int num) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    return 4.5;
}
void test5(std::unordered_map<int, std::string> m) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    m[709] = "Queen";
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
}
std::string test6(int i, float f, double d, std::string s, char c) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    s += c;
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    return s;
}
void test7(int i, float f, double d, std::string s, char c) {
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
    s += c;
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));
}

BOOST_AUTO_TEST_CASE(overloadedQueueTest)
{
    Implementation<float, int> i(2);
    int id1 = i.execute(test1, 111);
    int id2 = i.execute(test2, 222);
    int id3 = i.execute(test3, 333);
    int id4 = i.execute(test4, 444);

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    BOOST_CHECK_EQUAL(i.status(id1), FS_Running);
    BOOST_CHECK_EQUAL(i.status(id2), FS_Running);
    BOOST_CHECK_EQUAL(i.status(id3), FS_Queued );
    BOOST_CHECK_EQUAL(i.status(id4), FS_Queued );

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

    BOOST_CHECK_EQUAL(i.status(id1), FS_Ready);
    BOOST_CHECK_EQUAL(i.status(id2), FS_Ready);
    BOOST_CHECK_EQUAL(i.status(id3), FS_Running);
    BOOST_CHECK_EQUAL(i.status(id4), FS_Running);

    BOOST_CHECK_EQUAL(i.result(id1), 1.5);
    BOOST_CHECK_EQUAL(i.result(id2), 2.5);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

    BOOST_CHECK_EQUAL(i.result(id1), 1.5);
    BOOST_CHECK_EQUAL(i.result(id2), 2.5);
    BOOST_CHECK_EQUAL(i.result(id3), 3.5);
    BOOST_CHECK_EQUAL(i.result(id4), 4.5);
}

BOOST_AUTO_TEST_CASE(waitingTest)
{
    Implementation<float, int> i(2);
    int id1 = i.execute(test1, 111);
    int id2 = i.execute(test2, 222);
    
    boost::this_thread::sleep(boost::posix_time::milliseconds(500));

    BOOST_CHECK_EQUAL(i.status(id1), FS_Running);
    BOOST_CHECK_EQUAL(i.status(id2), FS_Running);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

    BOOST_CHECK_EQUAL(i.status(id1), FS_Ready);
    BOOST_CHECK_EQUAL(i.status(id2), FS_Ready);
    BOOST_CHECK_EQUAL(i.result(id1), 1.5);
    BOOST_CHECK_EQUAL(i.result(id2), 2.5);

    int id3 = i.execute(test3, 333);
    int id4 = i.execute(test4, 444);

    boost::this_thread::sleep(boost::posix_time::milliseconds(500));

    BOOST_CHECK_EQUAL(i.status(id3), FS_Running);
    BOOST_CHECK_EQUAL(i.status(id4), FS_Running);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));

    BOOST_CHECK_EQUAL(i.status(id3), FS_Ready);
    BOOST_CHECK_EQUAL(i.status(id4), FS_Ready);
    BOOST_CHECK_EQUAL(i.result(id3), 3.5);
    BOOST_CHECK_EQUAL(i.result(id4), 4.5);
}

BOOST_AUTO_TEST_CASE(voidSpecializationAndCompoundTypesTest)
{
    Implementation<void, std::unordered_map<int, std::string>> impl(1);
    
    std::unordered_map<int, std::string> m;
    m[111] = "I want to break free";
    m[222] = "I want to break free";
    m[333] = "I want to break free from your lies";
    m[444] = "You're so self satisfied I don't need you";
    m[555] = "I've got to break free";
    m[666] = "God knows, God knows I want to break free";
    
    int id = impl.execute(test5, m);

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    BOOST_CHECK_EQUAL(impl.status(id), FS_Running);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    
    BOOST_CHECK_EQUAL(impl.status(id), FS_Ready);
}

BOOST_AUTO_TEST_CASE(manyTypesTest)
{
    Implementation<std::string, int, float, double, std::string, char> impl(1);

    int id = impl.execute(test6, 1, 2.3, 4.5, "ok", 'a');

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    BOOST_CHECK_EQUAL(impl.status(id), FS_Running);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    
    BOOST_CHECK_EQUAL(impl.status(id), FS_Ready);

    BOOST_CHECK_EQUAL(impl.result(id), "oka");
}

BOOST_AUTO_TEST_CASE(voidSpecializationAndManyTypesTest)
{
    Implementation<void, int, float, double, std::string, char> impl(1);

    int id = impl.execute(test7, 1, 2.3, 4.5, "ok", 'a');

    boost::this_thread::sleep(boost::posix_time::milliseconds(100));

    BOOST_CHECK_EQUAL(impl.status(id), FS_Running);

    boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
    
    BOOST_CHECK_EQUAL(impl.status(id), FS_Ready);
}
