#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "Windows.h"
#include "utils.h"

using namespace std;

struct TextData {
    vector<char> text;
    unordered_map<char, int> chars;
    int len;
    float match_index;
};

TextData reference_text;

void print_top_characters(const TextData& text_data) {
    vector<pair<char, int>> elements(text_data.chars.begin(), text_data.chars.end());

    sort(elements.begin(), elements.end(),
        [](const pair<char, int>& a, const pair<char, int>& b) {
            return a.second > b.second;
        });

    cout << "Наиболее встречающиеся символы в тексте: " << endl;
    int count = 0;

    for (const auto& pair : elements) {
        if (count >= 5) break;
        cout << pair.first << ": " << pair.second << endl;
        ++count;
    }
}

TextData analyze_text_statistics(vector<char> v) {
    TextData text_data;
    text_data.text = v;
    text_data.chars = unordered_map<char, int>{};

    for (char byte : text_data.text) {
        text_data.chars[byte]++;
    }

    text_data.len = text_data.text.size();

    text_data.match_index = 0.0;

    if (text_data.len <= 1) {
        text_data.match_index = 0.0;
        return text_data;
    }

    double sum = 0.0;
    for (const auto& pair : text_data.chars) {
        double frequency = static_cast<double>(pair.second);
        sum += frequency * (frequency - 1.0);
    }

    double denominator = static_cast<double>(text_data.len) * static_cast<double>(text_data.len - 1);
    text_data.match_index = static_cast<float>(sum / denominator);

    return text_data;
}

char get_most_common_symbol(const TextData& text_data) {
    int max = 0;
    char most_char = 0;
    for (auto n : text_data.chars) {
        if (n.second > max) {
            max = n.second;
            most_char = n.first;
        }
    }
    return most_char;
}

void find_probable_key_lengths(string& input_file) {
    ifstream fin(input_file, ios::binary);
    if (!fin) {
        cerr << "Ошибка открытия входного файла." << endl;
        return;
    }
    vector<char> v((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
    vector<pair<int, float>> possible_lengths;

    float reference_index = reference_text.match_index;
    cout << "Индекс соответствия эталонного текста: " << reference_index << endl << endl;

    for (int key_len = 1; key_len <= 25; key_len++) {
        vector<char> v_h;
        for (size_t i = 0; i < v.size(); i++) {
            if (i % key_len == 0) { v_h.push_back(v[i]); }
        }
        TextData sampled_text = analyze_text_statistics(v_h);
        float current_index = sampled_text.match_index;
        cout << "Длина ключа: " << key_len << " Длина текста: " << sampled_text.len << " Индекс: " << current_index << endl;

        float difference = abs(current_index - reference_index);

        possible_lengths.push_back({ key_len , difference });
    }
    cout << endl;

    cout << "Наиболее вероятные длины ключа:" << endl;

    vector<int> probable_lengths;
    for (const auto& pair : possible_lengths) {
        if (pair.second > 0.01f) continue;
        probable_lengths.push_back(pair.first);
    }

    sort(probable_lengths.begin(), probable_lengths.end());

    int count = 0;
    for (int length : probable_lengths) {
        cout << "Длина ключа: " << length << endl;
        count++;
    }
}

void decipher(int len_key, TextData& encrypted_text) {
    char common_char = get_most_common_symbol(reference_text);
    cout << endl << "Эталонный текст: " << endl;
    print_top_characters(reference_text);

    cout << endl << "Зашифрованный текст: " << endl;
    print_top_characters(encrypted_text);
    cout << endl;

    vector<char> encrypted_data = encrypted_text.text;
    vector<vector<char>> vectors(len_key);
    for (size_t i = 0; i < encrypted_data.size(); i++) {
        vectors[i % len_key].push_back(encrypted_data[i]);
    }
    vector<TextData> texts;
    for (auto vec : vectors) {
        TextData text = analyze_text_statistics(vec);
        texts.push_back(text);
    }
    vector<char> bias;

    cout << "Анализ позиции ключа" << endl;
    for (int pos = 0; pos < len_key; pos++) {
        cout << endl << "Позиция ключа " << pos + 1 << "" << endl;

        TextData& current_text = texts[pos];
        auto chars_map = current_text.chars;
        int total_chars = current_text.len;

        vector<pair<char, int>> sorted_chars(chars_map.begin(), chars_map.end());
        sort(sorted_chars.begin(), sorted_chars.end(),
            [](const pair<char, int>& a, const pair<char, int>& b) {
                return a.second > b.second;
            });

        cout << "Самые частые символы:" << endl;
        for (int i = 0; i < min(5, (int)sorted_chars.size()); i++) {
            float percentage = (sorted_chars[i].second * 100.0f) / total_chars;
            cout << "  '" << sorted_chars[i].first << "' - "
                << sorted_chars[i].second << " раз ("
                << percentage << "%)" << endl;
        }
    }
    cout << endl;

    cout << "Ключ: ";
    for (auto text : texts) {
        char anchar = get_most_common_symbol(text);
        bias.push_back(anchar - common_char);
        cout << (char)(anchar - common_char);
    }
    cout << endl;

    for (size_t i = 0; i < encrypted_data.size(); i++) {
        encrypted_data[i] -= bias[i % bias.size()];
    }
    ofstream fout("decripered.txt", ios::binary);
    for (size_t i = 0; i < encrypted_data.size(); i++) {
        fout << encrypted_data[i];
    }
    cout << "Расшифрованный текст сохранен в decripered.txt" << endl;
}

int main(int argc, char* argv[]) {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    ifstream fin("cherry.txt", ios::binary);
    if (!fin) {
        cerr << "Ошибка открытия файла cherry.txt" << endl;
        return 1;
    }
    vector<char> v((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
    reference_text = analyze_text_statistics(v);

    while (1) {
        cout << "Выберите действие:" << endl;
        cout << "1. Считать эталонный текст и сохранить частоты" << endl;
        cout << "2. Определение длины ключа шифрованного текста" << endl;
        cout << "3. Расшифровка текста" << endl;
        cout << "0. Выход" << endl;

        switch (GetCorrectNumber(0, 3)) {
        case 0:
            return 0;
        case 1:
        {
            cout << "Введите файл с эталонным текстом: " << endl;
            string file = input_string();
            ifstream fin(file, ios::binary);
            if (!fin) {
                cerr << "Ошибка открытия файла: " << file << endl;
                break;
            }
            vector<char> v((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
            reference_text = analyze_text_statistics(v);
            cout << "Индекс соответствия: " << reference_text.match_index << endl;
            cout << "Длина текста: " << reference_text.len << endl;
            break;
        }
        case 2: {
            cout << "Введите файл с зашифрованным текстом: " << endl;
            string file = input_string();
            find_probable_key_lengths(file);
            break;
        }
        case 3: {
            cout << "Введите файл с зашифрованным текстом: " << endl;
            string file = input_string();
            ifstream fin(file, ios::binary);
            if (!fin) {
                cerr << "Ошибка открытия входного файла." << endl;
                break;
            }
            vector<char> v((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
            TextData encipher_text = analyze_text_statistics(v);

            cout << "Введите предполагаемую длину ключа: " << endl;
            int len = GetCorrectNumber(1, 25); 
            decipher(len, encipher_text);
            break;
        }
        default:
            break;
        }
    };
    return 0;
}