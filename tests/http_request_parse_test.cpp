#include "http.hpp"

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

TEST(HTTPRequestTest, ParsesValidGetRequest) {
  std::string data =
      "GET /index.html HTTP/1.1\r\nHost: example.com\r\nConnection: "
      "keep-alive\r\n\r\n";
  HTTPRequest req(data);

  EXPECT_EQ(req.get_method(), RequestMethod::GET);
  EXPECT_EQ(req.get_path(), "/index.html");
  EXPECT_EQ(req.get_host(), "example.com");
  EXPECT_EQ(req.get_version(), HTTPVersion::HTTP_1_1);
  EXPECT_TRUE(req.wants_keep_alive());
}

TEST(HTTPRequestTest, ParsesAllKnownMethods) {
  auto method_of = [](std::string_view m) {
    std::string data = std::string(m) + " / HTTP/1.1\r\n\r\n";
    return HTTPRequest(data).get_method();
  };
  EXPECT_EQ(method_of("GET"), RequestMethod::GET);
  EXPECT_EQ(method_of("POST"), RequestMethod::POST);
  EXPECT_EQ(method_of("HEAD"), RequestMethod::HEAD);
  EXPECT_EQ(method_of("PUT"), RequestMethod::PUT);
  EXPECT_EQ(method_of("OPTIONS"), RequestMethod::OPTIONS);
}

TEST(HTTPRequestTest, UnknownMethodIsReportedAsUnknown) {
  std::string data = "PATCH / HTTP/1.1\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_method(), RequestMethod::UNKNOWN);
}

TEST(HTTPRequestTest, UnknownVersionIsReportedAsUnknown) {
  std::string data = "GET / HTTP/2.0\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_version(), HTTPVersion::UNKNOWN);
}

TEST(HTTPRequestTest, Http11DefaultsToKeepAlive) {
  std::string data = "GET / HTTP/1.1\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_TRUE(req.wants_keep_alive());
}

TEST(HTTPRequestTest, Http10DefaultsToClose) {
  std::string data = "GET / HTTP/1.0\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_FALSE(req.wants_keep_alive());
}

TEST(HTTPRequestTest, Http10WithKeepAliveHeaderStaysAlive) {
  std::string data = "GET / HTTP/1.0\r\nConnection: keep-alive\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_TRUE(req.wants_keep_alive());
}

TEST(HTTPRequestTest, Http11WithCloseHeaderCloses) {
  std::string data = "GET / HTTP/1.1\r\nConnection: close\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_FALSE(req.wants_keep_alive());
}

TEST(HTTPRequestTest, ParsesIfNoneMatchHeader) {
  std::string data = "GET / HTTP/1.1\r\nIf-None-Match: \"abc123\"\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_if_none_match(), "\"abc123\"");
}

TEST(HTTPRequestTest, ParsesContentLengthHeader) {
  std::string data = "POST / HTTP/1.1\r\nContent-Length: 42\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_content_length(), 42u);
}

TEST(HTTPRequestTest, MissingContentLengthDefaultsToZero) {
  std::string data = "GET / HTTP/1.1\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_content_length(), 0u);
}

TEST(HTTPRequestTest, NonNumericContentLengthIsIgnored) {
  std::string data = "POST / HTTP/1.1\r\nContent-Length: nope\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_content_length(), 0u);
}

TEST(HTTPRequestTest, HeaderValueWithoutLeadingSpaceIsParsed) {
  std::string data = "GET / HTTP/1.1\r\nHost:example.com\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_host(), "example.com");
}

TEST(HTTPRequestTest, HeaderWithEmptyValueDoesNotThrow) {
  std::string data = "GET / HTTP/1.1\r\nHost: example.com\r\nX-Empty:\r\n\r\n";
  EXPECT_NO_THROW({ HTTPRequest req(data); });
}

TEST(HTTPRequestTest, HeaderLineWithNoColonIsIgnored) {
  std::string data = "GET / HTTP/1.1\r\nHost: example.com\r\nnot-a-header\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_host(), "example.com");
}

TEST(HTTPRequestTest, HeadersAreParsedInOrderAfterAMalformedOne) {
  std::string data = "GET / HTTP/1.1\r\nX:\r\nHost: example.com\r\n\r\n";
  HTTPRequest req(data);
  EXPECT_EQ(req.get_host(), "example.com");
}

TEST(HTTPRequestTest, EmptyRequestIsRejected) {
  std::string data;
  EXPECT_THROW({ HTTPRequest req(data); }, std::runtime_error);
}

TEST(HTTPRequestTest, RequestLineWithoutSpacesIsRejected) {
  std::string data = "GET\r\nHost: example.com\r\n\r\n";
  EXPECT_THROW({ HTTPRequest req(data); }, std::runtime_error);
}

TEST(HTTPRequestTest, RequestLineWithOnlyMethodAndPathIsRejected) {
  std::string data = "GET /\r\nHost: example.com\r\n\r\n";
  EXPECT_THROW({ HTTPRequest req(data); }, std::runtime_error);
}
