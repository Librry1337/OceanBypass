#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <thread>
#include <chrono>
#include <future>

// Глаз боба
std::unordered_map<DWORD, bool> checkedProcesses;

// Вайт лист для оптимизации
const std::unordered_set<std::wstring> systemProcesses = {
    L"System", L"smss.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe",
    L"services.exe", L"lsass.exe", L"svchost.exe", L"explorer.exe" L"vctip.exe"
};


void AnimateText(const std::string& text, int delay_ms) {
    for (char ch : text) {
        std::cout << ch << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    std::cout << std::endl;
}


void ClearConsole() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    if (!FillConsoleOutputCharacter(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten)) return;
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) return;
    SetConsoleCursorPosition(hConsole, coordScreen);
}

// ЕБАТЬ САТОШИ ДАЖЕ СЮДА ДОБРАЛСЯ
bool containsForbiddenString(const std::wstring& filePath, const std::vector<std::wstring>& forbiddenStrings) {
    try {
        std::wifstream file(filePath, std::ios::binary | std::ios::in);
        if (!file) {
            std::wcerr << L"Failed to open file: " << filePath << std::endl;
            return false;
        }

        const size_t bufferSize = 8192;  // Размер буфера
        std::vector<wchar_t> buffer(bufferSize);

        while (file.read(buffer.data(), bufferSize) || file.gcount() > 0) {
            std::wstring chunk(buffer.data(), file.gcount()); // Создаем строку из считанных данных
            for (const auto& str : forbiddenStrings) {
                if (chunk.find(str) != std::wstring::npos) {
                    return true;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return false;
}

// получаем название
std::wstring getProcessPath(DWORD processID) {
    std::wstring path;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) return path;

    wchar_t processPath[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH)) {
        path = processPath;
    }

    CloseHandle(hProcess);
    return path;
}

// Завершение процесса
bool terminateProcess(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (!hProcess) return false;

    bool result = TerminateProcess(hProcess, 0);
    CloseHandle(hProcess);
    return result;
}

// ускорение нахождения
void checkProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot! Error code: " << GetLastError() << std::endl;
        return;
    }

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);
    std::vector<std::future<void>> futures;

    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            // скип
            if (checkedProcesses.find(processEntry.th32ProcessID) != checkedProcesses.end()) {
                continue;
            }

            // скип
            if (systemProcesses.find(processEntry.szExeFile) != systemProcesses.end()) {
                continue;
            }

            // проверка в 3 ебла
            futures.push_back(std::async(std::launch::async, [processEntry]() {
                try {
                    std::wstring processPath = getProcessPath(processEntry.th32ProcessID);
                    if (!processPath.empty()) {
                        std::vector<std::wstring> forbiddenStrings = { L"Scanner from anticheat.ac", L"Process Hacker" };
                        if (containsForbiddenString(processPath, forbiddenStrings)) {
                            std::wcout << L"Process " << processEntry.szExeFile << L" (" << processEntry.th32ProcessID << L") contains forbidden string. Attempting to terminate..." << std::endl;
                            if (terminateProcess(processEntry.th32ProcessID)) {
                                std::wcout << L"Process " << processEntry.th32ProcessID << L" terminated successfully." << std::endl;
                            }
                            else {
                                std::wcout << L"Failed to terminate process " << processEntry.th32ProcessID << std::endl;
                            }
                        }
                        else {
                            std::wcout << L"Process " << processEntry.szExeFile << L" (" << processEntry.th32ProcessID << L") is safe. Path: " << processPath << std::endl;
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
                checkedProcesses[processEntry.th32ProcessID] = true;
                }));

        } while (Process32NextW(snapshot, &processEntry));
    }
    else {
        std::wcerr << L"Failed to retrieve first process entry! Error code: " << GetLastError() << std::endl;
    }

    for (auto& fut : futures) {
        fut.wait();  // Ожидаем завершения всех потоков
    }

    CloseHandle(snapshot);
}

// я хз зачем
void removeExitedProcesses() {
    std::unordered_set<DWORD> runningProcessIDs;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            runningProcessIDs.insert(processEntry.th32ProcessID);
        } while (Process32NextW(snapshot, &processEntry));
    }
    CloseHandle(snapshot);

    std::vector<DWORD> exitedProcesses;
    for (auto it = checkedProcesses.begin(); it != checkedProcesses.end(); ++it) {
        DWORD pid = it->first;
        if (runningProcessIDs.find(pid) == runningProcessIDs.end()) {
            exitedProcesses.push_back(pid);
        }
    }

    for (DWORD pid : exitedProcesses) {
        checkedProcesses.erase(pid);
    }
}

int main() {
   
    AnimateText("[~] Librry technology Inc.", 100);  // Ускоренная анимация

    
    ClearConsole();

   
    std::cout << "[~] Что делать будем?" << std::endl;
    std::cout << "[1] Запуск байпаса." << std::endl;
    std::cout << "[2] Debug." << std::endl;

  
    std::cout << "Выберите действие: ";
    std::string choice;
    std::getline(std::cin, choice);

    
    std::cout << "Выбранное действие: " << choice << std::endl;

    if (choice == "1") {
        std::cout << "Вы выбрали: Запуск байпаса." << std::endl;
        HWND hWnd = GetConsoleWindow();
        ShowWindow(hWnd, SW_HIDE);

        // Проверка процессов с уменьшенной задержкой
        while (true) {
            checkProcesses();
            removeExitedProcesses();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Минимальная задержка 10 мс
        }
    }
    else if (choice == "2") {
        std::cout << "Вы выбрали: Debug. Окно останется видимым, и начнется поиск..." << std::endl;

        while (true) {
            checkProcesses();
            removeExitedProcesses();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Минимальная задержка 10 мс
        }
    }
    else {
        std::cerr << "Неверный выбор. Попробуйте снова." << std::endl;
        return 1;
    }

    return 0;
}
