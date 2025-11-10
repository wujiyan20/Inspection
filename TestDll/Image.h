#pragma once
#ifdef PREIMAGEPROCESSINGLIB_EXPORTS
#define CPLUSPLUS_API __declspec(dllexport)
#else
#define CPLUSPLUS_API __declspec(dllimport)
#endif

#include <cstddef>
#include <opencv2/core.hpp>
using namespace cv;

struct DllInspectionResult {
	int class_id;
	float probability;
	cv::Rect bbox;
	const cv::Point* p_contour_data;
	size_t contour_size;

	bool toKeep; // Set to true if this result passes the filter, false otherwise.
};

void callbackLog(const char * message);

extern "C"
{

	CPLUSPLUS_API bool WINAPI Init(const char* sku_or_fileName);

	CPLUSPLUS_API bool WINAPI ExecuteInspection(
		DllInspectionResult* p_results_array,
		size_t num_results,
		float prob_threshold,
		float size_threshold,
		void* inputImageData,
		int rows, int cols, int type,
		const char*& outputImageData, //cropped and masked image for AI Inspection
		void*& outputDisplayImageData, //unmasked image for display only
		int& outputRows, int& outputCols, int& outputType);

	CPLUSPLUS_API bool WINAPI Close();

	CPLUSPLUS_API bool WINAPI IsInitialized();

	CPLUSPLUS_API void WINAPI SetLogCallback(void * callback);
}