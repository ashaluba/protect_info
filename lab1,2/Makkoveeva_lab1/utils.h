#pragma once
#include <iostream>
#include <string>

using namespace std;

template <typename T>
T GetCorrectNumber(T min, T max)
{
    T x;
    while ((cin >> x).fail()
        || cin.peek() != '\n'
        || x < min || x > max)
    {
        cin.clear();
        cin.ignore(10000, '\n');
        cout << "\nНеверный ввод!" << endl;
        cout << "Введите число от " << min << " до " << max << endl;
    }
    return x;
}

inline string input_string()
{
    cin >> ws;
    string str;
    getline(cin, str);
    return str;
}