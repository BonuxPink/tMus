/*
 * Copyright (C) 2023-2024 Dāniels Ponamarjovs <bonux@duck.com>
 *
 * This file is part of tMus.
 *
 * tMus is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * tMus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with tMus. If not, see <https://www.gnu.org/licenses/>.
 */

#include "ut.hpp"

#include "IniParser.hpp"

using namespace boost::ut;
using namespace boost::ut::bdd;

namespace ut = boost::ut;

int main()
{
    detail::cfg::abort_early = true;

    "Errors"_test = []
    {
        when ("There is an error in key = value") = []
        {
            then ("Exception should be thrown") = []
            {
                expect (throws ([]
                {
                    std::stringstream ss;
                    ss << "[SectionName] ; comment\nkey value"; // User forgot to add '='

                    IniParser p(ss);

                    expect (p.getSectionCount() == 1_ull);
                }));
            };
        };

        when ("There is a leftover character after section") = []
        {
            then ("A wild exception appears") = []
            {
                expect (throws ([]
                {
                    std::stringstream ss;
                    ss << "[SectionName]c\n"; // User misstyped 'c'
                    ss << "key = value\n";

                    IniParser p(ss);
                }));
            };
        };

        when ("Key's repeat") = []
        {
            then ("An exception should be thrown") = []
            {
                expect (throws ([]
                {
                    std::stringstream ss;
                    ss << "[SectionName] ; comment\nkey = value\nkey = value"; // key = value repeats twice. (3 if you count this comment)

                    IniParser p(ss);
                    expect (p.getSectionCount() == 1_ull);
                }));
            };
        };
    };

    "Test assignment"_test = []
    {
        given ("I have a ini file") = []
        {
            std::stringstream ss;
            ss << "[SectionName] ; comment\nkey = value";

            IniParser p(ss);
            expect (p.getSectionCount() == 1_ull);

            when ("I try to access non existent section") = [&]
            {
                then ("I get an exception") = [&]
                {
                    expect (throws ([&]
                    {
                        expect (p["RandomSectionName"].section_name == "SectionName");
                    }));
                };
            };

            when ("I try to access non existent key") = [&]
            {
                then ("I should get an exception") = [&]
                {
                    expect (throws ([&]
                    {
                        auto tmp = p["SectionName"]["RandomKey"].as<std::string>();
                    }));
                };
            };

            when ("I perform a valid fetch") = [&]
            {
                then ("It finds a key, and the value matches expected") = [&]
                {
                    auto tmp = p["SectionName"]["key"].as<std::string>();
                    expect (tmp == "value");
                };
            };

            when ("I perform a valid assignment") = [&]
            {
                p["SectionName"]["key"] = "newValue";
                expect (p["SectionName"]["key"].as<std::string>() == "newValue");
            };
        };
    };

    // If we modify the ini struct, we would like to save it back

    // To avoid repeating test cases, I made this one into a loop, passing this test case
    // '| std::tuple{ 1, 3.14f, std::string("Str") }' as an input
    using variant_type = std::variant<int, float, std::string>;
    "Save file"_test = []<typename T = variant_type>(auto typevalue)
    {
        auto my_to_string = [&](auto&& v) -> std::string
        {
            if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>)
            {
                return v;
            }
            // std::to_string add's trailing 0's, so we have to use something else
            else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, float>)
            {
                std::ostringstream os;
                os << v;
                return os.str();
            }
            else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, int>)
            {
                return std::to_string(v);
            }
            else
            {
                throw "This test accpets only std::string, int or float";
            }
        };

        std::string value;
        expect (not throws( [&]{ value = my_to_string(typevalue); } ));

        // It seems if we loop the section, ut complains that section's repeat
        given ("I have an ini file as that has key/value, where value is std::string") = [&]
        {
            const char* filename = "/tmp/tmus-test.ini";
            std::stringstream ss;

            std::string payload{ "[SectionName] ; comment\nkey = " };
            payload.append(value);

            ss << payload;

            then ("We put data into it and save") = [&]
            {

                IniParser p(ss);
                expect (p.getSectionCount() == 1_ull);

                p.SaveToFile(filename);

            };

            then ("Verify the saved file matches input") = [&]
            {
                // Verify what we have saved
                std::fstream fs{ filename };

                std::string line;

                std::getline(fs, line);
                expect (line == "[SectionName]");

                std::getline(fs, line);

                std::string check{ "key = " };
                check.append(value);

                expect (line == check);
            };
        };
    } | std::tuple{ 1, 3.14f, std::string("Str") };

    "Pass test"_test = []
    {
        std::stringstream ss;
        ss << "[Keybindings]\nk = up\nj = down\n";

        IniParser p(ss);

        expect (p["Keybindings"]["k"].as<std::string>() == "up");

        p["Keybindings"]["k"] = "down";

        auto temp = p["Keybindings"]["k"].as<std::string>();
        expect (temp == "down");

        p.SaveToFile("/tmp/tmus-test.ini");

        std::ifstream file{ "/tmp/tmus-test.ini" };
        expect (file.is_open());

        std::stringstream readFile;
        readFile << file.rdbuf();

        auto tmp = readFile.str();

        expect (tmp == "[Keybindings]\nj = down\nk = down\n");
    };
}
