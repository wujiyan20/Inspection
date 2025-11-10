// TestDll.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "dllloader.h"
#include <string>
#include "opencv2/core.hpp"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>  // C++17
#include "json.hpp"
#include "Image.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json; 
namespace fs = std::filesystem;


std::wstring dllPath = L"InspectionLib.dll";

typedef void(*MESSAGE_CALLBACK_FUNCTION)(const char*);
typedef bool (*InitType)(const char* sku_or_fileName);
typedef bool (*ExecuteInspectionType)(
	DllInspectionResult* p_results_array,
	size_t num_results,
	float prob_threshold,
	float size_threshold,
	void* inputImageData,
	int rows, int cols, int type,
	const char*& outputImageData, //cropped and masked image for AI Inspection
	void*& outputDisplayImageData, //unmasked image for display only
	int& outputRows, int& outputCols, int& outputType);
typedef bool (*CloseType)();
typedef bool (*IsInitializedType)();
typedef bool (*SetLogCallbackType)(MESSAGE_CALLBACK_FUNCTION callback);
void callbackLog(const char* message)
{
	std::cout << "Log: " << message << std::endl;
}

InitType InitFunc{ nullptr };
ExecuteInspectionType ExecuteInspectionFunc{ nullptr };
CloseType CloseFunc{ nullptr };
IsInitializedType IsInitializedFunc{ nullptr };
SetLogCallbackType SetLogCallbackFunc{ nullptr };

std::wstring string_to_wstring(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
	std::wstring wstr(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
	wstr.pop_back(); // remove null terminator added by WinAPI
	return wstr;
}

std::string wstring_to_string(const std::wstring& wstr) {
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size_needed, nullptr, nullptr);
	str.pop_back(); // remove null terminator added by WinAPI
	return str;
}

std::wstring getAbsolutePathRelativeToModule(const std::wstring& relative_filename, HMODULE hModule = NULL) {
	// Determine the max buffer size (32767 characters is the max path length in Windows)
	wchar_t path_buffer[32768];
	DWORD path_length = GetModuleFileNameW(hModule, path_buffer, 32768);

	if (path_length == 0 || path_length >= 32768) {
		// Handle error or path truncation
		return L"";
	}

	std::wstring full_path(path_buffer);

	// Find the last backslash (separating the directory from the filename)
	size_t last_slash = full_path.find_last_of(L"\\/");
	if (last_slash == std::wstring::npos) {
		// Should not happen for an EXE/DLL path, but return the filename if path is bad
		return relative_filename;
	}

	// Isolate the directory path and append the relative file name
	std::wstring directory_path = full_path.substr(0, last_slash + 1);
	return directory_path + relative_filename;
}

std::vector<DllInspectionResult> parseJsonResults(const std::string& filename) {
	std::vector<DllInspectionResult> results;

	try {
		std::wstring w_json_filename(filename.begin(), filename.end());
		std::wstring absolute_json_path = getAbsolutePathRelativeToModule(w_json_filename, NULL);
		std::string json_path(absolute_json_path.begin(), absolute_json_path.end());
		std::wcout << L"Attempting to open file at: " << absolute_json_path << std::endl;

		std::ifstream file_stream(absolute_json_path);
		if (!file_stream.is_open()) {
			std::cerr << "Error: Could not open file: " << filename << std::endl;
			return results;
		}

		json j;
		file_stream >> j;

		const auto& result_array = j["result"];

		for (const auto& item : result_array) {
			DllInspectionResult current_result;

			current_result.class_id = (item["class"] == "defect") ? 1 : 0;
			current_result.probability = static_cast<float>(item["score"]);

			if (item["box"].size() == 4) {
				current_result.bbox = cv::Rect(
					item["box"][0],
					item["box"][1],
					item["box"][2],
					item["box"][3]
				);
			}
			else {
				std::cerr << "Warning: Invalid bounding box format." << std::endl;
				continue;
			}
			
			std::vector<int> coords = item["contour"];
			if (coords.size() % 2 == 0) {
				current_result.contour_size = coords.size() / 2;
				std::vector<cv::Point> points;
				for (size_t i = 0; i < coords.size(); i += 2) {
					points.emplace_back(coords[i], coords[i + 1]);
				}
				cv::Point* contour_copy = new cv::Point[current_result.contour_size];
				std::copy(points.begin(), points.end(), contour_copy);
				current_result.p_contour_data = contour_copy;
			}
			else {
				std::cerr << "Warning: Contour point list has an odd number of coordinates." << std::endl;
				continue;
			}

			results.push_back(current_result);
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Error parsing JSON: " << e.what() << std::endl;
	}

	return results;
}

int main()
{
	std::cout << "current Path: '" << fs::current_path() << "'" << std::endl;
	DllLoader dllLoader;

	bool bSucesss = dllLoader.Load(dllPath);

	if (bSucesss)
	{
		InitFunc = dllLoader.GetFunction<InitType>("Init");
		std::cout << "load '" << wstring_to_string(dllPath) << "' OK" << std::endl;
		ExecuteInspectionFunc = dllLoader.GetFunction<ExecuteInspectionType>("ExecuteInspection");
		CloseFunc = dllLoader.GetFunction<CloseType>("Close");
		IsInitializedFunc = dllLoader.GetFunction<IsInitializedType>("IsInitialized");
		SetLogCallbackFunc = dllLoader.GetFunction<SetLogCallbackType>("SetLogCallback");

		SetLogCallbackFunc(callbackLog);
		std::cout << "load '" << wstring_to_string(dllPath) << "' functions OK" << std::endl;
	}
	else
	{
		std::cout << "load '" << wstring_to_string(dllPath) << "' fail" << std::endl;
		DWORD error_code = GetLastError();
		std::wcerr << L"LoadLibraryW failed with error code: " << error_code << std::endl;
	}

	if (InitFunc)
	{
		std::string sku_or_fileName = "Model A"; //recipe name or SKU
		bool bInit = InitFunc(sku_or_fileName.c_str());
		if (bInit)
		{
			std::cout << "Init OK" << std::endl;
		}
		else
		{
			std::cout << "Init fail" << std::endl;
		}
	}

	if (IsInitializedFunc)
	{
		bool bIsInit = IsInitializedFunc();
		if (bIsInit)
		{
			std::cout << "IsInitialized OK" << std::endl;
		}
		else
		{
			std::cout << "IsInitialized fail" << std::endl;
		}
	}

	if (ExecuteInspectionFunc)
	{
		const std::string filename = "sample.json";

		std::vector<DllInspectionResult> parsed_data = parseJsonResults(filename);

#pragma region print parsed json
	/*	std::cout << "--- Parsed Data Summary ---" << std::endl;
		std::cout << "Successfully parsed " << parsed_data.size() << " results." << std::endl;

		for (size_t i = 0; i < parsed_data.size(); ++i) {
			const auto& res = parsed_data[i];
			std::cout << "\nResult " << i + 1 << ":" << std::endl;
			std::cout << "  Class ID: " << res.class_id << std::endl;
			std::cout << "  Probability: " << res.probability << std::endl;
			std::cout << "  BBox (x, y, w, h): (" << res.bbox.x << ", " << res.bbox.y << ", "
				<< res.bbox.width << ", " << res.bbox.height << ")" << std::endl;
			std::cout << "  Contour Points Count: " << res.contour_size << std::endl;
			for (int i = 0; i < res.contour_size; i++) {
				std::cout << res.p_contour_data[i].x << ", " << res.p_contour_data[i].y << "; ";
			}
			std::cout << std::endl << "------------------" << std::endl;
		}*/
#pragma endregion

		const char* outputImageData = nullptr;
		void* outputDisplayImageData = nullptr;
		int outputRows = 0;
		int outputCols = 0;
		int outputType = 0;
		ExecuteInspectionFunc(parsed_data.data(), parsed_data.size(), 0.8, 0, nullptr, 0, 0, 0, outputImageData, outputDisplayImageData, outputRows, outputCols, outputType);

		for (size_t i = 0; i < parsed_data.size(); ++i) {
			const auto& res = parsed_data[i];
			if (!res.toKeep) continue;
			std::cout << "\nResult " << i + 1 << ":" << std::endl;
			std::cout << "  Class ID: " << res.class_id << std::endl;
			std::cout << "  Probability: " << res.probability << std::endl;
			std::cout << "  BBox (x, y, w, h): (" << res.bbox.x << ", " << res.bbox.y << ", "
				<< res.bbox.width << ", " << res.bbox.height << ")" << std::endl;
			std::cout << "  Contour Points Count: " << res.contour_size << std::endl;
			for (int i = 0; i < res.contour_size; i++) {
				std::cout << res.p_contour_data[i].x << ", " << res.p_contour_data[i].y << "; ";
			}
			std::cout << std::endl << "------------------" << std::endl;
		}
	}

	if (CloseFunc)
	{
		bool bClose = CloseFunc();
		if (bClose)
		{
			std::cout << "Close OK" << std::endl;
		}
		else
		{
			std::cout << "Close fail" << std::endl;
		}
	}
	std::cin.get();
	//cv::waitKey(0);


}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
