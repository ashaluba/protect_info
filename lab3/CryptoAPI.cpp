#pragma once

#include <windows.h>
#include <wincrypt.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")

using namespace std;

// Вспомогательные функции из framework.h
inline wstring GetLastErrorString(DWORD ErrorID = 0)
{
    if (!ErrorID)
        ErrorID = GetLastError();
    if (!ErrorID)
        return wstring();

    LPWSTR pBuff = nullptr;
    size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, ErrorID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&pBuff, 0, NULL);

    if (size == 0)
        return wstring();

    wstring s(pBuff, size);
    LocalFree(pBuff);

    return s;
}

inline void PrintError(const string& context = "")
{
    wstring error = GetLastErrorString();
    if (!context.empty())
        cerr << context << ": ";
    wcerr << error << endl;
}

// Чтение файла в вектор байт
vector<BYTE> ReadFileData(const string& filename)
{
    ifstream file(filename, ios::binary | ios::ate);
    if (!file)
        throw runtime_error("Cannot open file: " + filename);

    size_t size = file.tellg();
    file.seekg(0, ios::beg);

    vector<BYTE> buffer(size);
    if (!file.read((char*)buffer.data(), size))
        throw runtime_error("Cannot read file: " + filename);

    return buffer;
}

class CryptoAPI
{
    HCRYPTPROV m_hCP = NULL;
    HCRYPTKEY m_hExchangeKey = NULL;
    HCRYPTKEY m_hSessionKey = NULL;
    HCRYPTKEY m_hExportKey = NULL;

public:
    CryptoAPI()
    {
        if (!CryptAcquireContext(&m_hCP, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            if (!CryptAcquireContext(&m_hCP, NULL, NULL, PROV_RSA_AES, CRYPT_NEWKEYSET))
                PrintError("CryptAcquireContext");
    }

    ~CryptoAPI()
    {
        DestroyKeys();
        if (m_hCP)
            CryptReleaseContext(m_hCP, 0);
    }
    //создание AES ключа из пароля
    HCRYPTKEY DeriveKeyFromPassword(const string& password)
    {
        HCRYPTHASH hHash = 0;
        HCRYPTKEY hKey = 0;

        if (!CryptCreateHash(m_hCP, CALG_SHA_256, 0, 0, &hHash))
        {
            PrintError("CryptCreateHash");
            return 0;
        }

        if (!CryptHashData(hHash, (BYTE*)password.c_str(), (DWORD)password.length(), 0))
        {
            PrintError("CryptHashData");
            CryptDestroyHash(hHash);
            return 0;
        }

        if (!CryptDeriveKey(m_hCP, CALG_AES_256, hHash, 0, &hKey))
        {
            PrintError("CryptDeriveKey");
            CryptDestroyHash(hHash);
            return 0;
        }

        CryptDestroyHash(hHash);
        return hKey;
    }

    void DestroyKey(HCRYPTKEY& hKey)
    {
        if (hKey)
        {
            CryptDestroyKey(hKey);
            hKey = NULL;
        }
    }

    void DestroyKeys()
    {
        DestroyKey(m_hExchangeKey);
        DestroyKey(m_hSessionKey);
        DestroyKey(m_hExportKey);
    }

    // Генерация ключевой пары RSA
    bool CreateKeys(const string& password)
    {
        DWORD keySize = 2048;
        DWORD flags = CRYPT_EXPORTABLE | (keySize << 16);

        if (!CryptGenKey(m_hCP, CALG_RSA_KEYX, flags, &m_hExchangeKey))
        {
            PrintError("CryptGenKey RSA-2048");
            return false;
        }

        // Экспортируем публичный ключ
        DWORD pubSize = 0;
        if (!CryptExportKey(m_hExchangeKey, 0, PUBLICKEYBLOB, 0, NULL, &pubSize))
        {
            PrintError("CryptExportKey public (size)");
            return false;
        }

        vector<BYTE> pubKey(pubSize);
        if (!CryptExportKey(m_hExchangeKey, 0, PUBLICKEYBLOB, 0, pubKey.data(), &pubSize))
        {
            PrintError("CryptExportKey public (data)");
            return false;
        }

        // Создаем ключ из пароля
        m_hExportKey = DeriveKeyFromPassword(password);
        if (!m_hExportKey)
        {
            cerr << "Failed to create key from password" << endl;
            return false;
        }

        // Экспортируем приватный ключ, защищенный паролем
        DWORD privSize = 0;
        if (!CryptExportKey(m_hExchangeKey, m_hExportKey, PRIVATEKEYBLOB, 0, NULL, &privSize))
        {
            PrintError("CryptExportKey private (size)");
            return false;
        }

        vector<BYTE> privKey(privSize);
        if (!CryptExportKey(m_hExchangeKey, m_hExportKey, PRIVATEKEYBLOB, 0, privKey.data(), &privSize))
        {
            PrintError("CryptExportKey private (data)");
            return false;
        }

        ofstream pubFile("public.key", ios::binary);
        ofstream privFile("private.key", ios::binary);

        if (!pubFile || !privFile)
        {
            cerr << "Failed to create key files" << endl;
            return false;
        }

        pubFile.write((char*)pubKey.data(), pubSize);
        privFile.write((char*)privKey.data(), privSize);

        cout << "Keys saved to public.key and private.key" << endl;
        return true;
    }

    // Шифрование файла
    bool EncryptFile(const string& inputFile, const string& outputFile)
    {
        try
        {
            // Загружаем публичный ключ
            auto pubKeyData = ReadFileData("public.key");

            HCRYPTKEY hPublicKey = 0;
            if (!CryptImportKey(m_hCP, pubKeyData.data(), (DWORD)pubKeyData.size(), 0, 0, &hPublicKey))
            {
                PrintError("CryptImportKey public");
                return false;
            }

            // Читаем исходный файл
            auto fileData = ReadFileData(inputFile);

            // Генерируем сессионный AES ключ
            HCRYPTKEY hSessionKey = 0;
            if (!CryptGenKey(m_hCP, CALG_AES_256, CRYPT_EXPORTABLE, &hSessionKey))
            {
                PrintError("CryptGenKey session");
                CryptDestroyKey(hPublicKey);
                return false;
            }

            // Экспортируем сессионный ключ, зашифрованный публичным RSA ключом
            DWORD sessionKeySize = 0;
            if (!CryptExportKey(hSessionKey, hPublicKey, SIMPLEBLOB, 0, NULL, &sessionKeySize))
            {
                PrintError("CryptExportKey session (size)");
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPublicKey);
                return false;
            }

            vector<BYTE> sessionKey(sessionKeySize);
            if (!CryptExportKey(hSessionKey, hPublicKey, SIMPLEBLOB, 0, sessionKey.data(), &sessionKeySize))
            {
                PrintError("CryptExportKey session (data)");
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPublicKey);
                return false;
            }

            //Определяем размер зашифрованных данных
            DWORD encryptedSize = (DWORD)fileData.size();
            if (!CryptEncrypt(hSessionKey, 0, TRUE, 0, NULL, &encryptedSize, 0))
            {
                PrintError("CryptEncrypt (get size)");
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPublicKey);
                return false;
            }

            //Шифруем данные
            vector<BYTE> encryptedData(encryptedSize);
            memcpy(encryptedData.data(), fileData.data(), fileData.size());

            DWORD finalSize = (DWORD)fileData.size();
            if (!CryptEncrypt(hSessionKey, 0, TRUE, 0, encryptedData.data(), &finalSize, (DWORD)encryptedData.size()))
            {
                PrintError("CryptEncrypt");
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPublicKey);
                return false;
            }

            ofstream outFile(outputFile, ios::binary);
            if (!outFile)
            {
                cerr << "Cannot create output file: " << outputFile << endl;
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPublicKey);
                return false;
            }

            outFile.write((char*)&sessionKeySize, sizeof(DWORD));
            outFile.write((char*)sessionKey.data(), sessionKeySize);
            outFile.write((char*)&finalSize, sizeof(DWORD));
            outFile.write((char*)encryptedData.data(), finalSize);

            CryptDestroyKey(hSessionKey);
            CryptDestroyKey(hPublicKey);

            cout << "Encrypted: " << outputFile << endl;
            return true;
        }
        catch (const exception& e)
        {
            cerr << "Error: " << e.what() << endl;
            return false;
        }
    }

    //Дешифрование файла
    bool DecryptFile(const string& inputFile, const string& outputFile, const string& password)
    {
        try
        {
            //Загружаем приватный ключ
            auto privKeyData = ReadFileData("private.key");

            //Создаем ключ из пароля
            HCRYPTKEY hPasswordKey = DeriveKeyFromPassword(password);
            if (!hPasswordKey)
            {
                cerr << "Failed to create key from password" << endl;
                return false;
            }

            //Импортируем приватный ключ
            HCRYPTKEY hPrivateKey = 0;
            if (!CryptImportKey(m_hCP, privKeyData.data(), (DWORD)privKeyData.size(), hPasswordKey, 0, &hPrivateKey))
            {
                PrintError("CryptImportKey private");
                CryptDestroyKey(hPasswordKey);
                return false;
            }
            CryptDestroyKey(hPasswordKey);

            //Читаем зашифрованный файл
            ifstream inFile(inputFile, ios::binary);
            if (!inFile)
            {
                cerr << "Cannot open encrypted file: " << inputFile << endl;
                CryptDestroyKey(hPrivateKey);
                return false;
            }

            //Читаем сессионный ключ
            DWORD sessionKeySize = 0;
            inFile.read((char*)&sessionKeySize, sizeof(DWORD));

            vector<BYTE> sessionKey(sessionKeySize);
            inFile.read((char*)sessionKey.data(), sessionKeySize);

            //Читаем размер зашифрованных данных
            DWORD encryptedSize = 0;
            inFile.read((char*)&encryptedSize, sizeof(DWORD));

            //Читаем зашифрованные данные
            vector<BYTE> encryptedData(encryptedSize);
            inFile.read((char*)encryptedData.data(), encryptedSize);
            inFile.close();

            //Импортируем сессионный ключ
            HCRYPTKEY hSessionKey = 0;
            if (!CryptImportKey(m_hCP, sessionKey.data(), sessionKeySize, hPrivateKey, 0, &hSessionKey))
            {
                PrintError("CryptImportKey session");
                CryptDestroyKey(hPrivateKey);
                return false;
            }

            //Дешифруем данные
            DWORD decryptedSize = encryptedSize;
            if (!CryptDecrypt(hSessionKey, 0, TRUE, 0, encryptedData.data(), &decryptedSize))
            {
                PrintError("CryptDecrypt");
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPrivateKey);
                return false;
            }

            ofstream outFile(outputFile, ios::binary);
            if (!outFile)
            {
                cerr << "Cannot create output file: " << outputFile << endl;
                CryptDestroyKey(hSessionKey);
                CryptDestroyKey(hPrivateKey);
                return false;
            }

            outFile.write((char*)encryptedData.data(), decryptedSize);

            CryptDestroyKey(hSessionKey);
            CryptDestroyKey(hPrivateKey);

            cout << "Decrypted: " << outputFile << endl;
            return true;
        }
        catch (const exception& e)
        {
            cerr << "Error: " << e.what() << endl;
            return false;
        }
    }
};

void GenerateKeys(CryptoAPI& crypto, const string& password)
{
    if (!crypto.CreateKeys(password))
    {
        cerr << "Key generation failed!" << endl;
    }
}

void EncryptFile(CryptoAPI& crypto, const string& input, const string& output)
{
    if (!crypto.EncryptFile(input, output))
    {
        cerr << "Encryption failed!" << endl;
    }
}

void DecryptFile(CryptoAPI& crypto, const string& input, const string& output, const string& password)
{
    if (!crypto.DecryptFile(input, output, password))
    {
        cerr << "Decryption failed!" << endl;
    }
}


int main()
{
    CryptoAPI crypto;
    int choice;

    while (true)
    {
        cout << "1. Generate RSA-key pair" << endl;
        cout << "2. Encrypt file" << endl;
        cout << "3. Decrypt file" << endl;
        cout << "4. Exit" << endl;
        cout << "Choice: ";

        if (!(cin >> choice))
        {
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "Invalid input!" << endl;
            continue;
        }
        cin.ignore();

        if (choice == 1)
        {
            string password;
            cout << "Password for private key: ";
            getline(cin, password);
            if (password.empty())
            {
                cout << "Password cannot be empty!" << endl;
                continue;
            }
            GenerateKeys(crypto, password);
        }
        else if (choice == 2)
        {
            string input, output;
            cout << "Input file: ";
            getline(cin, input);
            cout << "Output file: ";
            getline(cin, output);
            EncryptFile(crypto, input, output);
        }
        else if (choice == 3)
        {
            string input, output, password;
            cout << "Input file: ";
            getline(cin, input);
            cout << "Output file: ";
            getline(cin, output);
            cout << "Password: ";
            getline(cin, password);
            DecryptFile(crypto, input, output, password);
        }
        else if (choice == 4)
        {
            break;
        }
        else
        {
            cout << "Invalid choice!" << endl;
        }
    }

    return 0;
}