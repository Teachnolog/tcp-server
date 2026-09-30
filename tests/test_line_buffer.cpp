#include "LineBuffer.hpp"

#include <gtest/gtest.h>

#include <string>

using tcp::LineBuffer;

TEST(LineBuffer, ReturnsNothingUntilLineIsComplete) {
    LineBuffer buffer(64);
    buffer.append("PIN");
    EXPECT_FALSE(buffer.popLine().has_value());

    buffer.append("G\n");
    EXPECT_EQ(buffer.popLine(), "PING");
    EXPECT_FALSE(buffer.popLine().has_value());
}

TEST(LineBuffer, SplitsSeveralLinesFromOneChunk) {
    LineBuffer buffer(64);
    buffer.append("SET a 1\nGET a\nPI");

    EXPECT_EQ(buffer.popLine(), "SET a 1");
    EXPECT_EQ(buffer.popLine(), "GET a");
    EXPECT_FALSE(buffer.popLine().has_value());
    EXPECT_EQ(buffer.bufferedBytes(), 2u);
}

TEST(LineBuffer, StripsCarriageReturn) {
    LineBuffer buffer(64);
    buffer.append("PING\r\n\r\n");
    EXPECT_EQ(buffer.popLine(), "PING");
    EXPECT_EQ(buffer.popLine(), "");
}

TEST(LineBuffer, DetectsUnterminatedLineOverLimit) {
    LineBuffer buffer(8);
    buffer.append("12345678");
    EXPECT_FALSE(buffer.overflowed());

    buffer.append("9");
    EXPECT_TRUE(buffer.overflowed());
    EXPECT_FALSE(buffer.popLine().has_value());
}

TEST(LineBuffer, DetectsCompleteLineOverLimit) {
    LineBuffer buffer(4);
    buffer.append("ok\n" + std::string(10, 'x') + "\n");

    EXPECT_EQ(buffer.popLine(), "ok");
    EXPECT_FALSE(buffer.popLine().has_value());
    EXPECT_TRUE(buffer.overflowed());
}

TEST(LineBuffer, RejectsZeroLimit) { EXPECT_THROW(LineBuffer(0), std::invalid_argument); }
