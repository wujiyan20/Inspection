#include "pch.h"
#include "Image.h"
#include <list>
#include <iostream>
#include <filesystem>
#include <queue>
#include <list>
#include "time.h"
#include "opencv2/core.hpp"
#include <opencv2/opencv.hpp>
#include <cmath>
#include <sstream>
#include <format>

struct OutputImageStruct {
	//cv::Mat OutputImage;
	cv::Mat DisplayImage;
};
#define COMPILED_MASK_MODE MASK_BOTTOMVIEW

// Find center of largest contour
cv::Point find_center(const cv::Mat& src) {
	cv::Mat gray;
	if (src.channels() == 3)
		cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
	else
		gray = src.clone();

	cv::Mat binary_mask;
	cv::threshold(gray, binary_mask, 254, 255, cv::THRESH_BINARY_INV);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (!contours.empty()) {
		auto largest = std::max_element(contours.begin(), contours.end(),
			[](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
				return cv::contourArea(a) < cv::contourArea(b);
			});

		cv::Moments m = cv::moments(*largest);
		if (m.m00 != 0)
			return cv::Point(static_cast<int>(m.m10 / m.m00), static_cast<int>(m.m01 / m.m00));
	}

	return cv::Point(0, 0);
}
cv::Point find_center(const cv::Mat& src, std::vector<cv::Point>& contour) {
	cv::Mat gray;
	if (src.channels() == 3)
		cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
	else
		gray = src.clone();

	cv::Mat binary_mask;
	cv::threshold(gray, binary_mask, 254, 255, cv::THRESH_BINARY_INV);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	if (!contours.empty()) {
		auto largest = std::max_element(contours.begin(), contours.end(),
			[](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
				return cv::contourArea(a) < cv::contourArea(b);
			});

		cv::Moments m = cv::moments(*largest);
		if (m.m00 != 0)
		{
			contour = *largest;
			return cv::Point(static_cast<int>(m.m10 / m.m00), static_cast<int>(m.m01 / m.m00));
		}
	}

	return cv::Point(0, 0);
}
// Min enclosing circle mask with white background
cv::Point find_center_contour(const cv::Mat& src) {
	cv::Mat gray;
	if (src.channels() == 3)
		cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
	else
		gray = src.clone();
	cv::Mat binary_mask;
	cv::threshold(gray, binary_mask, 254, 255, cv::THRESH_BINARY_INV);
	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
	if (!contours.empty()) {
		auto largest = std::max_element(contours.begin(), contours.end(),
			[](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
				return cv::contourArea(a) < cv::contourArea(b);
			});
		cv::Point2f center;
		float radius;
		cv::minEnclosingCircle(*largest, center, radius);
		return cv::Point(static_cast<int>(center.x), static_cast<int>(center.y));
	}
	return cv::Point(0, 0);
}

bool copyCircleImage(const cv::Mat& src, cv::Mat& dest, cv::Point offset, float circle_ratio = 0.85f, bool external = false)
{
	if (src.empty() || dest.empty()) {
		callbackLog("Error: Source or destination image is empty.");
		return false;
	}
	if (circle_ratio <= 0.0f || circle_ratio >= 1) {
		callbackLog("Error: Invalid circle ratios provided.");
		return false;
	}

	int width = dest.cols;
	int height = dest.rows;
	// Find object center from largest contour
	cv::Point center = cv::Point(width / 2, height / 2); // Default to center of destination image
	// Calculate radii
	float imageSize = static_cast<float>(std::min(width, height));
	int radius = static_cast<int>(imageSize * circle_ratio / 2.0f);

	// Create mask: white = masked, black = preserved
	cv::Mat mask(src.size(), CV_8UC1, cv::Scalar(external ? 255 : 0));
	// Paint circle
	cv::circle(mask, center + offset, radius, cv::Scalar(external ? 0 : 255), -1);

	cv::Rect srcRoi(
		cv::Point(std::max(0, center.x - (external ? width / 2 : radius) + offset.x), std::max(0, center.y - (external ? height / 2 : radius) + offset.y)),
		cv::Point(std::min(width, center.x + (external ? width / 2 : radius) + offset.x), std::min(height, center.y + (external ? height / 2 : radius) + offset.y))
	);

	cv::Rect destRoi = srcRoi - offset; // Adjust destination ROI by the offset

	src(srcRoi).copyTo(dest(destRoi), mask(srcRoi));

	return true;
}

bool copyRingImage(const cv::Mat& src, cv::Mat& dest, double& minPixelValue, double& maxPixelValue, cv::Point offset, float outer_circle_ratio = 0.85f, float inner_circle_ratio = 0.7f, bool invert = false)
{
	if (src.empty() || dest.empty()) {
		callbackLog("Error: Source or destination image is empty.");
		return false;
	}

	if (outer_circle_ratio <= inner_circle_ratio || outer_circle_ratio <= 0.0f || inner_circle_ratio <= 0.0f || outer_circle_ratio >= 1) {
		callbackLog("Error: Invalid circle ratios provided.");
		return false;
	}


	int width = dest.cols;
	int height = dest.rows;
	// Find object center from largest contour
	cv::Point center = cv::Point(width / 2, height / 2); // Default to center of destination image
	// Calculate radii
	float imageSize = static_cast<float>(std::min(width, height));
	int outer_radius = static_cast<int>(imageSize * outer_circle_ratio / 2.0f);
	int inner_radius = static_cast<int>(imageSize * inner_circle_ratio / 2.0f);

	// Create mask: white = masked, black = preserved
	cv::Mat mask(src.size(), CV_8UC1, cv::Scalar(0));  // start fully masked (white)
	// Draw outer circle (black): keep content
	cv::circle(mask, center + offset, outer_radius, cv::Scalar(255), -1);
	// Draw inner circle (white): mask out again
	cv::circle(mask, center + offset, inner_radius, cv::Scalar(0), -1);
	// Invert mask so black = mask, white = keep

	cv::Rect srcRoi(
		cv::Point(std::max(0, center.x - (invert ? width / 2 : outer_radius) + offset.x), std::max(0, center.y - (invert ? height / 2 : outer_radius) + offset.y)),
		cv::Point(std::min(width, center.x + (invert ? width / 2 : outer_radius) + offset.x), std::min(height, center.y + (invert ? height / 2 : outer_radius) + offset.y))
	);

	cv::minMaxLoc(src(srcRoi), &minPixelValue, &maxPixelValue, nullptr, nullptr, mask(srcRoi));

	cv::Rect destRoi = srcRoi - offset; // Adjust destination ROI by the offset

	if (invert)
	{
		cv::Mat inversedMask;
		cv::bitwise_not(mask, inversedMask);
		src(srcRoi).copyTo(dest(destRoi), inversedMask(srcRoi));
	}
	else
		src(srcRoi).copyTo(dest(destRoi), mask(srcRoi));

	return true;
}

typedef void(*MESSAGE_CALLBACK_FUNCTION)(const char*);

MESSAGE_CALLBACK_FUNCTION cbLog;

//ToDo - Declare your Variables or Objects here.
OutputImageStruct output;

bool bInitialized = false;
int filterSizeX = 5;
cv::Mat kernel1row;
cv::Mat inputImage;
cv::Mat loResInputImage;
cv::Mat grayLoResInputImage;
cv::Mat normalizedRingImage;
cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5));
cv::Mat closed_image;
cv::Mat opened_image;
cv::Size dstSize;

cv::Mat unwrapped;
cv::Mat rotatedUnwrapped;
cv::Mat hist;
cv::Mat colorUnwrapped;

std::string resultJsonStr;


void callbackLog(const char* message) {
	if (cbLog != NULL)
		cbLog(message);
}

CPLUSPLUS_API bool WINAPI Init(const char* sku_or_fileName)
{
	try {
		//ToDo - Instantiate/Initialize/Load Settings File/Prepare Image filter
		kernel1row = cv::getGaussianKernel(filterSizeX * 2 + 1, -1).t();

		for (int col = 0; col < filterSizeX; col++)
		{
			kernel1row.at<double>(0, col) = -kernel1row.at<double>(0, col);
		}
		kernel1row.at<double>(0, filterSizeX) = 0;

		bInitialized = true;



		return bInitialized;
	}
	catch (std::exception exp) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__) + std::string(exp.what());
		cbLog(msg.c_str());
	}
	catch (...) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__);
		cbLog(msg.c_str());
	}
	return false;
}

void normalizeImage(cv::Mat& image, cv::Mat& normImage, const int minVal, const int maxVal) {
	if (image.empty()) {
		callbackLog("Error: Image is empty.");
		return;
	}
	if (image.type() != CV_8UC1) {
		callbackLog("Error: Image type is not CV_8UC1.");
		return;
	}

	cv::threshold(image, normImage, maxVal, maxVal, cv::THRESH_TRUNC);
	cv::threshold(normImage, normImage, minVal, minVal, cv::THRESH_TOZERO);

	if (maxVal - minVal > 0) {
		image.convertTo(image, CV_8UC1, 1.0 / (maxVal - minVal), -minVal / (maxVal - minVal));
	}
	else {
		image.setTo(0); // If all pixels are the same, set to zero
	}
}

struct Region {
	int start;
	int end;
	int length;
	int center;
};

Region findWidestRegion(const std::vector<int>& hist, int threshold) {
	int n = hist.size();
	if (n == 0) return { -1,-1,0,-1 };

	int bestStart = -1, bestEnd = -1, bestLen = 0;
	int curStart = -1;

	// Step 1: scan normally
	for (int i = 0; i < n; i++) {
		if (hist[i] > threshold) {
			if (curStart == -1) curStart = i;
		}
		else {
			if (curStart != -1) {
				int curLen = i - curStart;
				if (curLen > bestLen) {
					bestLen = curLen;
					bestStart = curStart;
					bestEnd = i - 1;
				}
				curStart = -1;
			}
		}
	}

	// Step 2: check wrap-around region
	if (curStart != -1) {
		int wrapLen = (n - curStart);  // part at end
		int i = 0;
		while (i < n && hist[i] > threshold) { // count part at start
			wrapLen++;
			i++;
		}
		if (wrapLen > bestLen) {
			bestLen = wrapLen;
			bestStart = curStart;
			bestEnd = (i - 1 + n) % n;  // modulo for wrap-around
		}
	}

	// Step 3: compute center index
	int center = -1;
	if (bestLen > 0) {
		if (bestStart <= bestEnd) {
			center = bestStart + (bestEnd - bestStart) / 2;
		}
		else {
			center = (bestStart + ((bestEnd + n - bestStart) / 2)) % n;
		}
	}

	return { bestStart, bestEnd, bestLen, center };
}
std::string getMatTypeStr(int type) {
	std::string r;

	uchar depth = type & CV_MAT_DEPTH_MASK;
	uchar chans = 1 + (type >> CV_CN_SHIFT);

	switch (depth) {
	case CV_8U:  r = "8U"; break;
	case CV_8S:  r = "8S"; break;
	case CV_16U: r = "16U"; break;
	case CV_16S: r = "16S"; break;
	case CV_32S: r = "32S"; break;
	case CV_32F: r = "32F"; break;
	case CV_64F: r = "64F"; break;
	default:     r = "User"; break;
	}

	r += "C";
	r += (chans + '0');

	return r;
}

Region findRegion(const cv::Mat& hist)
{

	cv::Mat padded;
	cv::Mat paddedFilteredImage;
	cv::Mat filteredImage;

	cv::copyMakeBorder(hist, padded, 0, 0, filterSizeX, filterSizeX, cv::BORDER_WRAP);

	cv::filter2D(padded, paddedFilteredImage, -1, kernel1row, cv::Point(-1, -1), 0);
	filteredImage = paddedFilteredImage.colRange(filterSizeX, paddedFilteredImage.cols - filterSizeX);

	double minVal = filteredImage.at<double>(0, 0);
	double maxVal = filteredImage.at<double>(0, 0);
	cv::Point minLoc;
	cv::Point maxLoc;

	cv::minMaxLoc(filteredImage, &minVal, &maxVal, &minLoc, &maxLoc);

	//cbLog(std::format("min: {} @ {}, max: {} @ {}", minVal, minLoc.x, maxVal, maxLoc.x).c_str());
	return { maxLoc.x, minLoc.x, minLoc.x - maxLoc.x, (minLoc.x + maxLoc.x) / 2 };

}


CPLUSPLUS_API bool WINAPI ExecuteInspection(
	DllInspectionResult* p_results_array,
	size_t num_results,
	float prob_threshold,
	float size_threshold,
	void* inputImageData,
	int rows, int cols, int type,
	const char*& outputImageData, //cropped and masked image for AI Inspection
	void*& outputDisplayImageData, //unmasked image for display only
	int& outputRows, int& outputCols, int& outputType)
{
	try {
		for (size_t i = 0; i < num_results; ++i) {
			DllInspectionResult& result = p_results_array[i]; // Non-const reference

			if (result.probability < prob_threshold) {
				result.toKeep = false;
				continue;
			}

			const cv::Point* start_ptr = result.p_contour_data;
			size_t size = result.contour_size;

			if (size == 0 || start_ptr == nullptr) {
				result.toKeep = false;
				continue;
			}

			std::vector<cv::Point> contour(start_ptr, start_ptr + size);
			double area = cv::contourArea(contour);

			if (area >= size_threshold) {
				result.toKeep = true;
			}
			else {
				result.toKeep = false;
			}
		}

		return true;
	}
	catch (const cv::Exception& exp) {
		// Catch specific OpenCV exceptions
		std::string msg = std::string("OpenCV Exception in Func: ") + std::string(__FUNCTION__) + std::string(exp.what());
		cbLog(msg.c_str());
	}
	catch (std::exception exp) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__) + std::string(exp.what());
		cbLog(msg.c_str());
	}
	catch (...) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__);
		cbLog(msg.c_str());
	}

	return false;
}

CPLUSPLUS_API bool WINAPI Close()
{
	try {
		//ToDo - Destroy all objects


		bInitialized = false;
		return true;
	}
	catch (std::exception exp) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__) + std::string(exp.what());
		cbLog(msg.c_str());
	}
	catch (...) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__);
		cbLog(msg.c_str());
	}
	return false;
}

CPLUSPLUS_API bool WINAPI IsInitialized() {
	try {
		return bInitialized;
	}
	catch (std::exception exp) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__) + std::string(exp.what());
		cbLog(msg.c_str());
	}
	catch (...) {
		std::string msg = std::string("Exception Error in Func: ") + std::string(__FUNCTION__);
		cbLog(msg.c_str());
	}
	return false;
}

CPLUSPLUS_API void WINAPI SetLogCallback(void* callback) {
	cbLog = (MESSAGE_CALLBACK_FUNCTION)callback;
}

