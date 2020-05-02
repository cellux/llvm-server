#include "doctest.h"
#include "compiler.h"

static const char *src_add = \
  "define i32 @add(i32, i32) {\n"
  "  %3 = alloca i32\n"
  "  %4 = alloca i32\n"
  "  store i32 %0, i32* %3\n"
  "  store i32 %1, i32* %4\n"
  "  %5 = load i32, i32* %3\n"
  "  %6 = load i32, i32* %4\n"
  "  %7 = add i32 %5, %6\n"
  "  ret i32 %7\n"
  "}\n";

static const char *src_add_user =
  "declare i32 @add(i32, i32)\n"
  "define void @add_user(i32*) {\n"
  "  %2 = call i32 @add(i32 3, i32 2)\n"
  "  store i32 %2, i32* %0\n"
  "  ret void\n"
  "}\n";

ByteArray from_c_string(const char *str) {
  return ByteArray(str, str+strlen(str));
}

TEST_CASE("CompileContext") {
  CompileContext cc;
  cc.parse(from_c_string(src_add));
  cc.parse(from_c_string(src_add_user));
  cc.link();
  cc.commit();
  auto response = cc.call("add_user", 1);
  CHECK(response[0] == 5);
}

TEST_CASE("dump") {
  CompileContext cc;
  cc.parse(from_c_string(src_add));
  cc.parse(from_c_string(src_add_user));
  cc.link();
  auto bitcode = cc.dump();
  CHECK(bitcode.size() > 0);
  SUBCASE("commit+call after dump") {
    cc.commit();
    auto response = cc.call("add_user", 1);
    CHECK(response[0] == 5);
  }
  SUBCASE("parse into a new context, then commit+call") {
    CompileContext cc2;
    cc2.parse(bitcode);
    cc2.commit();
    auto response = cc2.call("add_user", 1);
    CHECK(response[0] == 5);
  }
}
