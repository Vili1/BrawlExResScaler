#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <tchar.h> 
#include <vector>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

//some global vars
HWND hGameWindow = FindWindow(NULL, "Brawlhalla");
char moduleName[] = "Adobe AIR.dll";
char moduleName2[] = "gameoverlayrenderer.dll";
DWORD pID = NULL;
HANDLE processHandle = NULL;
uintptr_t XtoScaleAddress = NULL;
uintptr_t FPSAddress = NULL;
int XtoScaleValDef, YtoScaleValDef, XtoScaleVal, YtoScaleVal;
int scale = 100;
float FPSval = 0;
float TargetFPS = 70;
float BuffFPS = 25;
int LoopSpeed = 100;
int AddVal = 15;
int SubVal = 15;
int PrefVer = 0;
int DownScaleLimit = 25;

// loads the config
void loadConfig() {
    std::ifstream configFile("Rescfg.txt");
    if (configFile.is_open()) {
        std::string line;
        while (std::getline(configFile, line)) {
            std::istringstream iss(line);
            std::string key;
            if (std::getline(iss, key, '=')) {
                std::string value;
                if (std::getline(iss, value)) {
                    // remove any potential whitespace
                    key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
                    value.erase(std::remove_if(value.begin(), value.end(), isspace), value.end());

                    // parse values based on key
                    if (key == "PrefVer") { // 1 = normal version, 2 = tech-test version
                        PrefVer = std::stoi(value);
                    }
                    else if (key == "TargetFPS") { // your fps target it can be anything below 1000 , preferably the refresh rate of ur monitor
                        TargetFPS = std::stof(value);
                    }
                    else if (key == "BuffFPS") { // thats an offset so once the res is downscaled and the fps recovers , it doesn't upscale back to the previous ress and cause lag spikes
                        BuffFPS = std::stof(value);
                    }
                    else if (key == "LoopSpeed") { // controlling the main loop speed in milli seconds, it effects only the speed which ur game is being down scaled
                        LoopSpeed = std::stoi(value);
                    }
                    else if (key == "AddVal") { // thats how much the program is allowed to upscale per every loop iteration 
                        AddVal = std::stoi(value);
                    }
                    else if (key == "SubVal") { // thats how much the program is allowed to downscale per every loop iteration
                        SubVal = std::stoi(value);
                    }
                    else if (key == "DownScaleLimit") { // this makes it so the resolution doesn't drop under a set limit, anything below 1% breaks the game rendering
                        DownScaleLimit = std::stoi(value);
                    }
                }
            }
        }
        configFile.close();
    }
    else {
        std::cerr << "Failed to open Rescfg.txt. Using default values." << std::endl;
    }
}


//gets the base addr of the module
uintptr_t dwGetModuleBaseAddress(TCHAR* lpszModuleName, uintptr_t pID) {
    uintptr_t dwModuleBaseAddress = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pID);
    MODULEENTRY32 ModuleEntry32 = { 0 };
    ModuleEntry32.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(hSnapshot, &ModuleEntry32)) {
        do {
            if (_tcscmp(ModuleEntry32.szModule, lpszModuleName) == 0) {
                dwModuleBaseAddress = (uintptr_t)ModuleEntry32.modBaseAddr;
                break;
            }
        } while (Module32Next(hSnapshot, &ModuleEntry32));

    }
    CloseHandle(hSnapshot);
    return dwModuleBaseAddress;
}

//finds the game window and spits out errors if its not fond
void findGameWindow() {
    if (hGameWindow != NULL) {
        std::cout << "Brawlhalla found successfully!" << "\n";
        GetWindowThreadProcessId(hGameWindow, &pID);
        processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pID);
        if (processHandle == INVALID_HANDLE_VALUE || processHandle == NULL) {
            std::cout << "Try to run the application as administrator." << "\n";
            Sleep(3000);
            exit(EXIT_FAILURE);
        }
    }
    else {
        std::cout << "Unable to find Brawlhalla, Please make sure that the game is opened!" << "\n";
        Sleep(3000);
        exit(EXIT_FAILURE);
    }
}

//closes the pogram when the game isn't open
void checkGameToExit() {
    HWND hGameWindowToExit = FindWindow(NULL, "Brawlhalla");
    if (hGameWindowToExit == NULL) {
        CloseHandle(processHandle);
        exit(EXIT_FAILURE);
    }
}

//my crappy function that reads a multilevel pointer
uintptr_t iniPRT(char moduleName[], uintptr_t offsetGameToBaseAddress, std::vector<uintptr_t> Offsets) {
    uintptr_t gameBaseAddress = dwGetModuleBaseAddress(_T(moduleName), pID);
    uintptr_t baseAddress = NULL;
    ReadProcessMemory(processHandle, (LPVOID)(gameBaseAddress + offsetGameToBaseAddress), &baseAddress, sizeof(baseAddress), NULL);
    uintptr_t Address = baseAddress;
    for (int i = 0; i < Offsets.size() - 1; i++) {
        ReadProcessMemory(processHandle, (LPVOID)(Address + Offsets.at(i)), &Address, sizeof(Address), NULL);
    }
    Address += Offsets.at(Offsets.size() - 1);
    return Address;
}

//my crappy res sacle function that calculates the resolution based on a % scale
std::pair<int, int> resScale(float Fwidth, float Fheight) {
    int pixel = Fwidth * Fheight / 100 * scale;
    float Wratio = Fheight / Fwidth;
    float Hratio = Fwidth / Fheight;
    int width = sqrt(pixel / Wratio);
    int height = sqrt(pixel / Hratio);
    return std::make_pair(width, height);
}

//the menu function, just a crappy cout stuff 
void menu() {
    while (true) {
        system("cls");
        std::cout << "Brawlhalla external dynamic resolution by Vili" << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "Current cfg" << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "Preferred game version: " << PrefVer << "\n";
        std::cout << "Target FPS: " << TargetFPS << "\n";
        std::cout << "Buffer FPS: " << BuffFPS << "\n";
        std::cout << "Loop Speed: " << LoopSpeed << "\n";
        std::cout << "Upscale %: " << AddVal << "\n";
        std::cout << "Downscale %: " << SubVal << "\n";
        std::cout << "Downscale limit%: " << DownScaleLimit << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "Current stats" << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "Default resolution: " << XtoScaleValDef << "x" << YtoScaleValDef << "\n";
        std::cout << "Current resolution: " << XtoScaleVal << "x" << YtoScaleVal << "\n";
        std::cout << "Current resolution scale in %: " << scale << "\n";
        std::cout << "Current FPS: " << FPSval << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void initMain() {
    loadConfig();
    findGameWindow();
    int ptrSelect = 0;

    if (PrefVer != 0) {
        ptrSelect = PrefVer;
    }
    else {
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "Select a version" << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cout << "1. Normal version" << "\n";
        std::cout << "2. Tech-test version" << "\n";
        std::cout << "-----------------------------------------------------------" << "\n";
        std::cin >> ptrSelect;
    }
    //select game version if config isn't pressent
    if (ptrSelect == 1) {
        SetConsoleTitle("Normal version");
        XtoScaleAddress = iniPRT(moduleName, 0x01331740, { 0x28, 0x14, 0x154, 0x14, 0x78, 0x50, 0x34, 0x2A4 });
    }
    else if (ptrSelect == 2) {
        SetConsoleTitle("Tech-test version");
        XtoScaleAddress = iniPRT(moduleName, 0x012DAACC, { 0x170, 0x268, 0x78, 0x50, 0x34, 0x2B8 });
    }
    else {
        std::cout << "Invalid value" << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        exit(EXIT_FAILURE);
    }
    //init the pointer to the FPS address
    FPSAddress = iniPRT(moduleName2, 0x00130F3C, { 0x18 });
    //reading the game resolution and storing it to X and Y ScaleValDef so we can reset back to default res
    ReadProcessMemory(processHandle, (LPCVOID)(XtoScaleAddress), &XtoScaleValDef, sizeof(int), NULL);
    ReadProcessMemory(processHandle, (LPCVOID)(XtoScaleAddress + 4), &YtoScaleValDef, sizeof(int), NULL);
    //copying the def res to new variables that can be modified throughout the code
    XtoScaleVal = XtoScaleValDef;
    YtoScaleVal = YtoScaleValDef;
    //menu thread
    std::thread(menu).detach();
}


int main() {
    initMain();

    //main loop
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(LoopSpeed));
        ReadProcessMemory(processHandle, (LPCVOID)(FPSAddress), &FPSval, sizeof(float), NULL); // reading the FPS every loop, sadly that value doesn't update that fast in the dll 

        if (FPSval < TargetFPS && scale > DownScaleLimit) {
            scale -= SubVal;
            std::pair<int, int> Scaleresult = resScale(XtoScaleValDef, YtoScaleValDef);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress), &Scaleresult.first, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress - 132), &Scaleresult.first, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress + 4), &Scaleresult.second, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress - 128), &Scaleresult.second, sizeof(int), 0);
            XtoScaleVal = Scaleresult.first;
            YtoScaleVal = Scaleresult.second;
        }

        if (FPSval > TargetFPS + BuffFPS && scale < 100) {
            scale += AddVal;
            std::pair<int, int> Scaleresult = resScale(XtoScaleValDef, YtoScaleValDef);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress), &Scaleresult.first, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress - 132), &Scaleresult.first, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress + 4), &Scaleresult.second, sizeof(int), 0);
            WriteProcessMemory(processHandle, (LPVOID)(XtoScaleAddress - 128), &Scaleresult.second, sizeof(int), 0);
            XtoScaleVal = Scaleresult.first;
            YtoScaleVal = Scaleresult.second;
        }
        checkGameToExit();
    }
    return 0;
}
//XtoScaleAddress= XtoScaleAddressCPY
//Xinternal = XtoScaleAddressCPY - 132;
//YtoScale = XtoScaleAddressCPY + 4;
//Yinternal = XtoScaleAddressCPY - 128;
