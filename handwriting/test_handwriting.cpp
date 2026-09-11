#include <metasequoia/handwriting.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace metasequoia::handwriting;
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    Recognizer recognizer(argv[1]);
    // Human pen trajectories: 中, with the central vertical last.
    std::vector<Stroke> ink = {
        {{35, 40}, {35, 105}}, {{35, 40}, {125, 40}, {125, 105}}, {{35, 105}, {125, 105}}, {{80, 15}, {80, 140}}};
    auto words = recognizer.recognize(ink, 160, 155);
    for (const auto &word : words)
        std::cout << word << '\n';
    if (std::find(words.begin(), words.end(), "中") == words.end())
        return 2;
    auto moved = ink;
    for (auto &stroke : moved)
        for (auto &point : stroke)
        {
            point.x = point.x * 0.7f + 180;
            point.y = point.y * 0.7f + 10;
        }
    const auto moved_words = recognizer.recognize(moved, 400, 155);
    if (std::find(moved_words.begin(), moved_words.end(), "中") == moved_words.end())
        return 7;
    if (!recognizer.recognize({}, 160, 155).empty())
        return 3;
    try
    {
        recognizer.recognize({{{-1, 0}}}, 160, 155);
        return 4;
    }
    catch (const std::invalid_argument &)
    {
    }
    try
    {
        recognizer.recognize(ink, 0, 155);
        return 5;
    }
    catch (const std::invalid_argument &)
    {
    }
    try
    {
        Recognizer missing("/missing-handwriting-model");
        return 6;
    }
    catch (const std::runtime_error &)
    {
    }
}
