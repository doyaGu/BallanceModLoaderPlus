#include "StringUtils.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <locale>
#include <algorithm>

class StringUtilsTest : public ::testing::Test {
protected:
    // Helper function to convert string vector to wstring vector
    std::vector<std::wstring> ToWStringVector(const std::vector<std::string>& strs) {
        std::vector<std::wstring> result;
        for (const auto& s : strs) {
            result.push_back(utils::ToWString(s));
        }
        return result;
    }
};

// Test string splitting
TEST_F(StringUtilsTest, SplitString) {
    // Test with std::string
    {
        std::string test = "one,two,three,four";
        auto result = utils::SplitString(test, ",");
        ASSERT_EQ(4, result.size());
        EXPECT_EQ("one", result[0]);
        EXPECT_EQ("two", result[1]);
        EXPECT_EQ("three", result[2]);
        EXPECT_EQ("four", result[3]);
    }
    
    // Test with empty delimiters
    {
        std::string test = "onetwothreefour";
        auto result = utils::SplitString(test, "");
        ASSERT_EQ(1, result.size());
        EXPECT_EQ("onetwothreefour", result[0]);
    }
    
    // Test with empty string
    {
        std::string test = "";
        auto result = utils::SplitString(test, ",");
        ASSERT_EQ(0, result.size());
    }
    
    // Test with consecutive delimiters
    {
        std::string test = "one,,two,three,,four";
        auto result = utils::SplitString(test, ",");
        ASSERT_EQ(6, result.size());
        EXPECT_EQ("one", result[0]);
        EXPECT_EQ("", result[1]);
        EXPECT_EQ("two", result[2]);
        EXPECT_EQ("three", result[3]);
        EXPECT_EQ("", result[4]);
        EXPECT_EQ("four", result[5]);
    }
    
    // Test with wide string
    {
        std::wstring test = L"one,two,three,four";
        auto result = utils::SplitString(test, L",");
        ASSERT_EQ(4, result.size());
        EXPECT_EQ(L"one", result[0]);
        EXPECT_EQ(L"two", result[1]);
        EXPECT_EQ(L"three", result[2]);
        EXPECT_EQ(L"four", result[3]);
    }
    
    // Test with multi-character delimiter
    {
        std::string test = "one::two::three::four";
        auto result = utils::SplitString(test, "::");
        ASSERT_EQ(4, result.size());
        EXPECT_EQ("one", result[0]);
        EXPECT_EQ("two", result[1]);
        EXPECT_EQ("three", result[2]);
        EXPECT_EQ("four", result[3]);
    }
}

// Test string trimming
TEST_F(StringUtilsTest, TrimString) {
    // Test TrimString
    {
        std::string test = "  hello world  ";
        utils::TrimString(test);
        EXPECT_EQ("hello world", test);
    }
    
    // Test TrimStringCopy
    {
        std::string test = "  hello world  ";
        auto result = utils::TrimStringCopy(test);
        EXPECT_EQ("hello world", result);
        EXPECT_EQ("  hello world  ", test); // Original unchanged
    }
    
    // Test with no spaces
    {
        std::string test = "hello";
        utils::TrimString(test);
        EXPECT_EQ("hello", test);
    }
    
    // Test with only spaces
    {
        std::string test = "   ";
        utils::TrimString(test);
        EXPECT_EQ("", test);
    }
    
    // Test with empty string
    {
        std::string test = "";
        utils::TrimString(test);
        EXPECT_EQ("", test);
    }
    
    // Test with wide string
    {
        std::wstring test = L"  hello world  ";
        utils::TrimString(test);
        EXPECT_EQ(L"hello world", test);
    }
    
    // Test with various whitespace characters
    {
        std::string test = " \t\n\r\f\v hello world \t\n\r\f\v ";
        utils::TrimString(test);
        EXPECT_EQ("hello world", test);
    }
}

// Test string joining
TEST_F(StringUtilsTest, JoinString) {
    // Test with std::string
    {
        std::vector<std::string> strs = {"one", "two", "three", "four"};
        auto result = utils::JoinString(strs, ",");
        EXPECT_EQ("one,two,three,four", result);
    }
    
    // Test with single character delimiter
    {
        std::vector<std::string> strs = {"one", "two", "three", "four"};
        auto result = utils::JoinString(strs, ',');
        EXPECT_EQ("one,two,three,four", result);
    }
    
    // Test with empty vector
    {
        std::vector<std::string> strs;
        auto result = utils::JoinString(strs, ",");
        EXPECT_EQ("", result);
    }
    
    // Test with single element
    {
        std::vector<std::string> strs = {"one"};
        auto result = utils::JoinString(strs, ",");
        EXPECT_EQ("one", result);
    }
    
    // Test with empty elements
    {
        std::vector<std::string> strs = {"one", "", "three", ""};
        auto result = utils::JoinString(strs, ",");
        EXPECT_EQ("one,,three,", result);
    }
    
    // Test with wide string
    {
        std::vector<std::wstring> strs = {L"one", L"two", L"three", L"four"};
        auto result = utils::JoinString(strs, L",");
        EXPECT_EQ(L"one,two,three,four", result);
    }
}

// Test case conversion
TEST_F(StringUtilsTest, CaseConversion) {
    // Test ToLower
    {
        std::string test = "Hello World 123";
        auto result = utils::ToLower(test);
        EXPECT_EQ("hello world 123", result);
    }
    
    // Test ToUpper
    {
        std::string test = "Hello World 123";
        auto result = utils::ToUpper(test);
        EXPECT_EQ("HELLO WORLD 123", result);
    }
    
    // Test with already lowercase
    {
        std::string test = "hello world";
        auto result = utils::ToLower(test);
        EXPECT_EQ("hello world", result);
    }
    
    // Test with already uppercase
    {
        std::string test = "HELLO WORLD";
        auto result = utils::ToUpper(test);
        EXPECT_EQ("HELLO WORLD", result);
    }
    
    // Test with empty string
    {
        std::string test = "";
        EXPECT_EQ("", utils::ToLower(test));
        EXPECT_EQ("", utils::ToUpper(test));
    }
    
    // Test with wide string
    {
        std::wstring test = L"Hello World 123";
        EXPECT_EQ(L"hello world 123", utils::ToLower(test));
        EXPECT_EQ(L"HELLO WORLD 123", utils::ToUpper(test));
    }
}

// Test string comparison functions
TEST_F(StringUtilsTest, StringComparison) {
    // Test StartsWith
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::StartsWith(test, "Hello"));
        EXPECT_FALSE(utils::StartsWith(test, "hello"));
        EXPECT_TRUE(utils::StartsWith(test, "hello", false)); // Case insensitive
        EXPECT_FALSE(utils::StartsWith(test, "World"));
    }
    
    // Test EndsWith
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::EndsWith(test, "World"));
        EXPECT_FALSE(utils::EndsWith(test, "world"));
        EXPECT_TRUE(utils::EndsWith(test, "world", false)); // Case insensitive
        EXPECT_FALSE(utils::EndsWith(test, "Hello"));
    }
    
    // Test Contains
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::Contains(test, "lo Wo"));
        EXPECT_FALSE(utils::Contains(test, "lo wo"));
        EXPECT_TRUE(utils::Contains(test, "lo wo", false)); // Case insensitive
        EXPECT_FALSE(utils::Contains(test, "Goodbye"));
    }
    
    // Test with empty strings
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::StartsWith(test, ""));
        EXPECT_TRUE(utils::EndsWith(test, ""));
        EXPECT_TRUE(utils::Contains(test, ""));
        
        test = "";
        EXPECT_TRUE(utils::StartsWith(test, ""));
        EXPECT_TRUE(utils::EndsWith(test, ""));
        EXPECT_TRUE(utils::Contains(test, ""));
        EXPECT_FALSE(utils::StartsWith(test, "Hello"));
        EXPECT_FALSE(utils::EndsWith(test, "World"));
        EXPECT_FALSE(utils::Contains(test, "Hello"));
    }
    
    // Test with wide strings
    {
        std::wstring test = L"Hello World";
        EXPECT_TRUE(utils::StartsWith(test, L"Hello"));
        EXPECT_TRUE(utils::EndsWith(test, L"World"));
        EXPECT_TRUE(utils::Contains(test, L"lo Wo"));
    }
}

// Test string conversion functions
TEST_F(StringUtilsTest, StringConversion) {
    // Test UTF-8 <-> UTF-16 conversion
    {
        std::string utf8 = "Hello World";
        std::wstring utf16 = utils::Utf8ToUtf16(utf8);
        std::string back = utils::Utf16ToUtf8(utf16);
        EXPECT_EQ(utf8, back);
    }
    
    // Test with empty strings
    {
        std::string utf8 = "";
        std::wstring utf16 = utils::Utf8ToUtf16(utf8);
        EXPECT_TRUE(utf16.empty());
        
        utf16 = L"";
        utf8 = utils::Utf16ToUtf8(utf16);
        EXPECT_TRUE(utf8.empty());
    }
    
    // Test with non-ASCII characters
    {
        std::string utf8 = "Hello 你好 World";
        std::wstring utf16 = utils::Utf8ToUtf16(utf8);
        std::string back = utils::Utf16ToUtf8(utf16);
        EXPECT_EQ(utf8, back);
    }
    
    // Test ANSI <-> UTF-16 conversion
    {
        std::string ansi = "Hello World";
        std::wstring utf16 = utils::AnsiToUtf16(ansi);
        std::string back = utils::Utf16ToAnsi(utf16);
        EXPECT_EQ(ansi, back);
    }
    
    // Test C-style string overloads
    {
        const char* utf8 = "Hello World";
        std::wstring utf16 = utils::Utf8ToUtf16(utf8);
        const wchar_t* wstr = utf16.c_str();
        std::string back = utils::Utf16ToUtf8(wstr);
        EXPECT_EQ(std::string(utf8), back);
    }
    
    // Test with nullptr
    {
        std::wstring utf16 = utils::Utf8ToUtf16(nullptr);
        EXPECT_TRUE(utf16.empty());
        
        std::string utf8 = utils::Utf16ToUtf8(nullptr);
        EXPECT_TRUE(utf8.empty());
    }
}

// Test string hash function
TEST_F(StringUtilsTest, StringHash) {
    // Test with std::string
    {
        std::string s1 = "Hello";
        std::string s2 = "Hello";
        std::string s3 = "hello";
        
        EXPECT_EQ(utils::HashString(s1), utils::HashString(s2));
        EXPECT_NE(utils::HashString(s1), utils::HashString(s3));
    }
    
    // Test with C-style strings
    {
        const char* s1 = "Hello";
        const char* s2 = "Hello";
        const char* s3 = "hello";
        
        EXPECT_EQ(utils::HashString(s1), utils::HashString(s2));
        EXPECT_NE(utils::HashString(s1), utils::HashString(s3));
    }
    
    // Test with wide strings
    {
        std::wstring s1 = L"Hello";
        std::wstring s2 = L"Hello";
        std::wstring s3 = L"hello";
        
        EXPECT_EQ(utils::HashString(s1), utils::HashString(s2));
        EXPECT_NE(utils::HashString(s1), utils::HashString(s3));
    }
    
    // Test with empty and nullptr
    {
        std::string empty = "";
        EXPECT_NE(0, utils::HashString(empty));
        
        const char* null_ptr = nullptr;
        EXPECT_EQ(0, utils::HashString(null_ptr));
    }
}

// Test legacy compatibility functions
TEST_F(StringUtilsTest, LegacyCompatibility) {
    // Test StringStartsWith/EndsWith
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::StringStartsWith(test, "Hello"));
        EXPECT_TRUE(utils::StringEndsWith(test, "World"));
        EXPECT_FALSE(utils::StringStartsWith(test, "hello"));
        EXPECT_FALSE(utils::StringEndsWith(test, "world"));
    }
    
    // Test case-insensitive versions
    {
        std::string test = "Hello World";
        EXPECT_TRUE(utils::StringStartsWithCaseInsensitive(test, "hello"));
        EXPECT_TRUE(utils::StringEndsWithCaseInsensitive(test, "world"));
    }
    
    // Test wide string versions
    {
        std::wstring test = L"Hello World";
        EXPECT_TRUE(utils::StringStartsWith(test, L"Hello"));
        EXPECT_TRUE(utils::StringEndsWith(test, L"World"));
        EXPECT_TRUE(utils::StringStartsWithCaseInsensitive(test, L"hello"));
        EXPECT_TRUE(utils::StringEndsWithCaseInsensitive(test, L"world"));
    }
}

// Test ANSI stripping covers CSI/OSC/DCS/SOS/PM/APC and single ESC forms
TEST_F(StringUtilsTest, StripAnsiCodes) {
    // Simple SGR coloring
    {
        std::string s = std::string("Hello ") + "\x1b[31m" + "Red" + "\x1b[0m" + " World";
        EXPECT_EQ("Hello Red World", utils::StripAnsiCodes(s.c_str()));
    }

    // Multiple CSI sequences
    {
        std::string s = std::string("Start ") + "\x1b[2J" + "\x1b[H" + "Done";
        EXPECT_EQ("Start Done", utils::StripAnsiCodes(s.c_str()));
    }

    // OSC with BEL terminator
    {
        std::string s = std::string("A ") + "\x1b]0;MyTitle\x07" + " B";
        EXPECT_EQ("A  B", utils::StripAnsiCodes(s.c_str()));
    }

    // OSC with ST (ESC \\) terminator
    {
        std::string s = std::string("X ") + "\x1b]1337;url=http://example/\x1b\\" + " Y";
        EXPECT_EQ("X  Y", utils::StripAnsiCodes(s.c_str()));
    }

    // DCS ... ST
    {
        std::string s = std::string("L ") + "\x1bPqabc\x1b\\" + " R";
        EXPECT_EQ("L  R", utils::StripAnsiCodes(s.c_str()));
    }

    // PM ... ST
    {
        std::string s = std::string("P ") + "\x1b^payload\x1b\\" + " Q";
        EXPECT_EQ("P  Q", utils::StripAnsiCodes(s.c_str()));
    }

    // APC ... ST
    {
        std::string s = std::string("I ") + "\x1b_payload\x1b\\" + " J";
        EXPECT_EQ("I  J", utils::StripAnsiCodes(s.c_str()));
    }

    // 8-bit CSI (C1) form
    {
        std::string s = std::string("x ") + "\x9B31m" + "RED" + "\x9B0m" + " y";
        EXPECT_EQ("x RED y", utils::StripAnsiCodes(s.c_str()));
    }

    // 8-bit OSC with ST
    {
        std::string s = std::string("x ") + "\x9D0;title\x9C" + " y";
        EXPECT_EQ("x  y", utils::StripAnsiCodes(s.c_str()));
    }

    // 8-bit DCS with ST
    {
        std::string s = std::string("x ") + "\x90some-data\x9C" + " y";
        EXPECT_EQ("x  y", utils::StripAnsiCodes(s.c_str()));
    }

    // Common two-byte ESC forms (RI/NEL/HTS/DEC save/restore)
    {
        std::string s = std::string("a ") + "\x1bM" + " b " + "\x1bE" + " c";
        EXPECT_EQ("a  b  c", utils::StripAnsiCodes(s.c_str()));
    }

    // Charset/designate and DEC line attributes (3-byte typical)
    {
        std::string s = std::string("u ") + "\x1b(B" + " v " + "\x1b#8" + " w";
        EXPECT_EQ("u  v  w", utils::StripAnsiCodes(s.c_str()));
    }

    // Unknown ESC fallthrough: drop ESC and the next char
    {
        std::string s = std::string("left ") + "\x1b`" + " right";
        EXPECT_EQ("left  right", utils::StripAnsiCodes(s.c_str()));
    }
}

// Main function that runs all the tests
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
