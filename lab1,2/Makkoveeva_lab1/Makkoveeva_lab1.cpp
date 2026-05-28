//#include <iostream>
//#include <fstream>
//#include <string>
//
//using namespace std;
//
//
//void vigenere(const string& inputFile, const string& outputFile, const string& key, bool encrypt)
//{
//    ifstream inFile(inputFile, ios::binary);
//    ofstream outFile(outputFile, ios::binary);
//
//    char ch;
//    int keyIndex = 0;
//    int keyLength = key.length();
//
//    while (inFile.get(ch)) {
//        char currentChar = static_cast<char>(ch);
//        char keyChar = static_cast<char>(key[keyIndex]);
//
//        if (encrypt) {
//            char encryptedChar = (currentChar + keyChar) % 256;
//            outFile.put(encryptedChar);
//        }
//        else {
//            char decryptedChar = (currentChar - keyChar + 256) % 256;
//            outFile.put(decryptedChar);
//        }
//
//        keyIndex = (keyIndex + 1) % keyLength;
//    }
//}
//
//int main()
//{
//    setlocale(LC_ALL, "rus");
//
//    string inputFile, outputFile, key;
//    int choice;
//
//    cout << "1. Зашифровать текст" << endl;
//    cout << "2. Дешифровать текст" << endl;
//    cout << "Выберите действие ";
//    cin >> choice;
//    cin.ignore();
//
//    if (choice != 1 && choice != 2) {
//        cerr << "Выберите 1 или 2" << endl;
//        return 1;
//    }
//
//    cout << "Введите имя входного файла: ";
//    getline(cin, inputFile);
//
//    cout << "Введите имя выходного файла: ";
//    getline(cin, outputFile);
//
//    cout << "Введите ключ: ";
//    getline(cin, key);
//
//    bool encrypt = (choice == 1);
//    vigenere(inputFile, outputFile, key, encrypt);
//
//    return 0;
//}