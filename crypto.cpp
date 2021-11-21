#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <random>

#include "globals.h"

using namespace std;

#include <openssl/sha.h>

string sha256(const string str)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, str.c_str(), str.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

string generatePreImage() {
    std::random_device rd;
    std::default_random_engine gen(rd());

    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        //"!\xA3$%^&*():@~#'?/><.,|`\xAC\xA6";

    string preImage;
    preImage.reserve(PREIMAGE_SIZE);
    std::uniform_int_distribution<int> random_number(0, strlen(charset) - 1);

    for( int i = 0; i < PREIMAGE_SIZE; i++ ) {
        preImage.push_back(charset[random_number(gen)]);
    }

    return preImage;
}



