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

// Ищу процессы
std::unordered_map<DWORD, bool> checkedProcesses;


void AnimateText(const std::string& text, int delay_ms) {
    for (char ch : text) {
        std::cout << ch << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    std::cout << std::endl;
}

// Функция для очистки консоли
void ClearConsole() {
  
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD coordScreen = { 0, 0 };
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;

    // Получаем количество символов в буфере консоли
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        return;
    }

    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

   // так надо бро
    if (!FillConsoleOutputCharacter(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten)) {
        return;
    }

    // сосал?
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) {
        return;
    }

    // сам хз зачем
    SetConsoleCursorPosition(hConsole, coordScreen);
}

// Проверка на спайс aka didyoumuch
bool containsForbiddenString(const std::wstring& filePath, const std::vector<std::wstring>& forbiddenStrings) {
    std::wifstream file(filePath, std::ios::binary);
    if (!file) {
        std::wcerr << L"Failed to open file: " << filePath << std::endl;
        return false;
    }

    std::wstring buffer;
    file.seekg(0, std::ios::end);
    buffer.resize(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(&buffer[0], buffer.size());

    for (const auto& str : forbiddenStrings) {
        if (buffer.find(str) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

// ВЫВОД НАЗВАНИЯ ПРОЦЕССА
std::wstring getProcessPath(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

    if (!hProcess) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to open process ID: " << processID << L". Error code: " << error << std::endl;
        return L"";
    }

    wchar_t processPath[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH)) {
        CloseHandle(hProcess);
        return std::wstring(processPath);
    }

    DWORD error = GetLastError();
    std::wcerr << L"Failed to get module file name for process ID: " << processID << L". Error code: " << error << std::endl;

    CloseHandle(hProcess);
    return L"";
}

// Убиваю на павал
bool terminateProcess(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);

    if (!hProcess) {
        DWORD error = GetLastError();
        std::wcerr << L"Failed to open process for termination. Process ID: " << processID << L". Error code: " << error << std::endl;
        return false;
    }

    if (!TerminateProcess(hProcess, 0)) {
        std::wcerr << L"Failed to terminate process ID: " << processID << L". Error code: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    std::wcout << L"Process " << processID << L" terminated successfully." << std::endl;
    CloseHandle(hProcess);
    return true;
}

//это пизда
void checkProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot! Error code: " << GetLastError() << std::endl;
        return;
    }

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &processEntry)) {
        do {
            if (checkedProcesses.find(processEntry.th32ProcessID) != checkedProcesses.end()) {
                continue;
            }

            std::wstring processPath = getProcessPath(processEntry.th32ProcessID);

            if (!processPath.empty()) {
                std::vector<std::wstring> forbiddenStrings = { L"Scanner from anticheat.ac", L"Process Hacker" };
                if (containsForbiddenString(processPath, forbiddenStrings)) {
                    std::wcout << L"Process " << processEntry.szExeFile << L" (" << processEntry.th32ProcessID << L") contains forbidden string. Attempting to terminate..." << std::endl;
                    terminateProcess(processEntry.th32ProcessID);
                }
                else {
                    std::wcout << L"Process " << processEntry.szExeFile << L" (" << processEntry.th32ProcessID << L") is safe. Path: " << processPath << std::endl;
                }

                checkedProcesses[processEntry.th32ProcessID] = true;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }
    else {
        std::wcerr << L"Failed to retrieve first process entry! Error code: " << GetLastError() << std::endl;
    }

    CloseHandle(snapshot);
}


void removeExitedProcesses() {
    std::unordered_set<DWORD> runningProcessIDs;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::wcerr << L"Failed to create process snapshot for removal! Error code: " << GetLastError() << std::endl;
        return;
    }

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
        std::wcout << L"Removing exited process: " << pid << std::endl;
        checkedProcesses.erase(pid);
    }
}

int main() {
    // Что то на пиздатом
    AnimateText("[~] Librry technology Inc.", 100);  // Ускоренная анимация

    
    ClearConsole();

    // Выберай что буш
    std::cout << "[~] Что делать будем?" << std::endl;
    std::cout << "[1] Запуск байпаса." << std::endl;
    std::cout << "[2] Debug." << std::endl;

   
    std::cout << "Выберите действие: ";
    std::string choice;
    std::getline(std::cin, choice);

    // Еще одна проверка 
    std::cout << "Выбранное действие: " << choice << std::endl;

    if (choice == "1") {
        std::cout << "Вы выбрали: Запуск байпаса." << std::endl;
        // Самое крутое
        HWND hWnd = GetConsoleWindow();
        ShowWindow(hWnd, SW_HIDE);

        
        while (true) {
            checkProcesses();
            removeExitedProcesses();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Пауза уменьшена до 100 мс
        }
    }
    else if (choice == "2") {
        std::cout << "Вы выбрали: Debug. Окно останется видимым, и начнется поиск..." << std::endl;

        
        while (true) {
            checkProcesses();
            removeExitedProcesses();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Пауза уменьшена до 100 мс
        }
    }
    else {
        std::cerr << "Неверный выбор. Попробуйте снова." << std::endl;
        return 1;
    }

    return 0;
}