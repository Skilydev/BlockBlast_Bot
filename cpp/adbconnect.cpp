#include "adbconnect.h"

#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

string ADBConnect::command(const string& cmd) {
    system((cmd + " > output.txt").c_str());
    
    ifstream file("output.txt");
    string output((istreambuf_iterator<char>(file)),
                       istreambuf_iterator<char>());

    remove("output.txt");
    return output;
}

string ADBConnect::adb_command(const string& device, const string& cmd) {
    if (device == "") {
        cout << "[adbconnect/simulation] La commande ne peut pas être exécutée" << endl;
        return "";
    }

    const string full_command = "adb -s " + device + " " + cmd;
    return command(full_command);
}

string ADBConnect::choose_device() {
    string device = "";
    vector<string> list_devices;

    const string out = command("adb devices");
    const regex pattern(R"(([^\r\n\t]+)\t([^\s]+))");

    cout << "[adbconnect] List of devices attached" << endl;

    sregex_iterator next(out.begin(), out.end(), pattern);
    sregex_iterator end;

    while (next != end) {
        smatch match = *next;
        if (match[2].str() == "device") {
            cout << "[adbconnect] [" << list_devices.size() << "] ";
            list_devices.push_back(match[1].str());
        }
        else {
            cout << "[adbconnect] [X] ";
        }
        cout << match[1].str()
             << '\t'
             << match[2].str()
             << endl;
            
        next++;
    }

    if ((int)list_devices.size() > 0) {
        string response = "";
        cout << endl << "[adbconnect] Quel appareil voulez vous utiliser [digit or \'none\'] ? ";
        cin >> response;
        if (response == "none") {
            cout << "[adbconnect] Aucun appareil connecté" << endl << "[adbconnect] Mode simulation activé" << endl;
        }
        else {
            const int id_device = stoi(response);
            device = list_devices.at(id_device);
            
            cout << "[adbconnect] Connecté à : " << device << endl;
        }
    }
    else {
        cout << "[adbconnect] Aucun appareil utilisable atuellement..."
             << endl << endl
             << "[adbconnect] Voulez vous continuer en mode simulation [Y/n] ? ";
        string response = "";
        cin >> response;
        if (response == "y" or response == "Y") {
            cout << "[adbconnect] Aucun appareil connecté" << endl << "[adbconnect] Mode simulation activé" << endl;
        }
        else {
            exit(0);
        }
    }

    return device;
}

string ADBConnect::captureScreen(const string& device, const string& parentfolder) {
    if (device == "") {    
        string name = "";
        cout << "[adbconnect/simulation] Quel fichier local souhaitez vous ouvrir ? ";
        cin >> name;

        if (name == "" or name == "\n" or name == "\r\n") {
            name = "screen.png";
        }

        return name;
    }
    else {
        string name = "";
        cout << "[adbconnect] Sous quel nom souhaitez vous enregistrer la capture ? ";
        cin >> name;

        if (name == "" or name == "\n" or name == "\r\n") {
            name = "screen.png";
        }

        const string cmd = "adb -s " + device + " exec-out screencap -p";
        FILE* pipe = _popen(cmd.c_str(), "rb");
        ofstream f(parentfolder + name, ios::binary);

        char buffer[4096];
        size_t n;

        while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
            f.write(buffer, n);
        }
        
        pclose(pipe);
        f.close();

        cout << "[adbconnect] Image enregistrée dans : " << parentfolder + name << endl;
        return name;
    }
}

void ADBConnect::click(const string& device, const int& x, const int& y) {
    if (device == "") {
        cout << "[adbconnect/simulation] clic x:"
             << to_string(x)
             << " y:"
             << to_string(y)
             << endl;
        return;
    }

    const string cmd = "shell input tap " + to_string(x) + " " + to_string(y);
    adb_command(device, cmd);
    cout << "[adbconnect] clic x:"
             << to_string(x)
             << " y:"
             << to_string(y)
             << endl;

    return;
}