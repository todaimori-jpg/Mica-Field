// SPDX-License-Identifier: AGPL-3.0-only
#include "../Source/Dsp.h"
#include <iomanip>
#include <iostream>

int main()
{
    std::cout << std::setprecision(9) << "{\"params\":[";
    bool comma = false;
    for (const auto& p : mica::parameters())
    {
        if (comma) std::cout << ',';
        comma = true;
        std::cout << "{\"id\":" << std::quoted(p.id) << ",\"name\":" << std::quoted(p.name)
                  << ",\"initial\":" << p.initial << ",\"choices\":" << p.choices
                  << ",\"modTarget\":" << (p.modTarget ? "true" : "false") << '}';
    }
    std::cout << "],\"factory\":[";
    comma = false;
    for (const auto& p : mica::factory())
    {
        if (comma) std::cout << ',';
        comma = true;
        std::cout << "{\"name\":" << std::quoted(std::string(p.name))
                  << ",\"family\":" << std::quoted(std::string(p.family))
                  << ",\"description\":" << std::quoted(std::string(p.description)) << ",\"values\":[";
        bool inner = false;
        for (auto value : p.patch.values)
        {
            if (inner) std::cout << ',';
            inner = true;
            std::cout << value;
        }
        std::cout << "],\"routes\":[";
        inner = false;
        for (auto r : p.patch.routes)
        {
            if (inner) std::cout << ',';
            inner = true;
            std::cout << '[' << r.source << ',' << r.target << ',' << r.amount << ']';
        }
        std::cout << "]}";
    }
    std::cout << "],\"waves\":[";
    for (int frame = 0; frame < 6; ++frame)
    {
        if (frame) std::cout << ',';
        std::cout << '[';
        for (int i = 0; i < 129; ++i)
        {
            if (i) std::cout << ',';
            std::cout << mica::oscillatorTable(frame, float(i) / 128.f);
        }
        std::cout << ']';
    }
    std::cout << "]}";
}
