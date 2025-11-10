#ifndef DLLLOADER_H
#define DLLLOADER_H

#pragma once

#include <windows.h>
#include <string>
#include <stdexcept>

// Simple wrapper class to load a DLL and retrieve function pointers.
class DllLoader {
public:
    // Constructor initializes the module handle to nullptr.
    DllLoader() : m_hModule(nullptr) {}

    // Disable copy construction and assignment.
    DllLoader(const DllLoader&) = delete;
    DllLoader& operator=(const DllLoader&) = delete;

    // Allow move semantics.
    DllLoader(DllLoader&& other) noexcept : m_hModule(other.m_hModule) {
        other.m_hModule = nullptr;
    }
    DllLoader& operator=(DllLoader&& other) noexcept {
        if (this != &other) {
            Unload();
            m_hModule = other.m_hModule;
            other.m_hModule = nullptr;
        }
        return *this;
    }

    // Loads a DLL from the specified path.
    // Returns true if the DLL was loaded successfully.
    bool Load(const std::wstring& dllPath) {
        m_hModule = ::LoadLibraryW(dllPath.c_str());
        return (m_hModule != nullptr);
    }

    // Template method to get the address of an exported function.
    // Throws an exception if the DLL is not loaded or the function is not found.
    template<typename FuncType>
    FuncType GetFunction(const std::string& funcName) {
        if (!m_hModule) {
            throw std::runtime_error("DLL not loaded.");
        }
        FARPROC procAddress = ::GetProcAddress(m_hModule, funcName.c_str());
        if (!procAddress) {
            throw std::runtime_error("Function not found: " + funcName);
        }
        return reinterpret_cast<FuncType>(procAddress);
    }

    // Frees the loaded DLL.
    void Unload() {
        if (m_hModule) {
            ::FreeLibrary(m_hModule);
            m_hModule = nullptr;
        }
    }
    bool isLoaded()
    {
        return m_hModule;
    }

    // Destructor automatically frees the DLL.
    ~DllLoader() {
        Unload();
    }

private:
    HMODULE m_hModule {nullptr}; // Handle to the loaded DLL.
};

#endif // DLLLOADER_H
