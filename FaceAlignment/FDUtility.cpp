#include "stdafx.h"
#include "FDUtility.h"
#include "FDCVInclude.h"
#include <fstream>
#include <stdarg.h>

//#define SAVE_TRAIN_DATA_TO_FILE 

void FDBoundingBox::CalcCenter()
{
	m_centerX = m_x + m_width / 2.0;
	m_centerY = m_y + m_height / 2.0;
}

uint64 FDUtility::GetUInt64Value()
{
	return cv::getTickCount();
}

uint64 FDUtility::GetCurrentTime()
{
	return (uint64)((double)cv::getTickCount() / (cv::getTickFrequency() / 1000.0));
}

void FDUtility::SimilarityTransform(const cv::Mat_<double> &shape1, const cv::Mat_<double> &shape2, cv::Mat_<double> &rotation, double &scale)
{
	// todo how to calc similarity
	rotation = cv::Mat::zeros(2, 2, CV_64FC1);
	scale = 0;

	double centerX1 = 0, centerY1 = 0, centerX2 = 0, centerY2 = 0;
	for (int i = 0; i < shape1.rows; i++)
	{
		centerX1 += shape1(i, 0);
		centerY1 += shape1(i, 1);
		centerX2 += shape2(i, 0);
		centerY2 += shape2(i, 1);
	}
	centerX1 /= shape1.rows;
	centerY1 /= shape1.rows;
	centerX2 /= shape2.rows;
	centerY2 /= shape2.rows;

	cv::Mat_<double> temp1 = shape1.clone();
	cv::Mat_<double> temp2 = shape2.clone();
	for (int i = 0; i < shape1.rows; i++)
	{
		temp1(i, 0) -= centerX1;
		temp1(i, 1) -= centerY1;
		temp2(i, 0) -= centerX2;
		temp2(i, 1) -= centerY2;
	}

	cv::Mat_<double> covariance1, covariance2;
	cv::Mat_<double> mean1, mean2;
	cv::calcCovarMatrix(temp1, covariance1, mean1, CV_COVAR_COLS);
	cv::calcCovarMatrix(temp2, covariance2, mean2, CV_COVAR_COLS);

	double s1 = sqrt(cv::norm(covariance1));
	double s2 = sqrt(cv::norm(covariance2));
	temp1 = 1.0 / s1 * temp1;
	temp2 = 1.0 / s2 * temp2;

	double num = 0;
	double den = 0;
	for (int i = 0; i < shape1.rows; i++)
	{
		num = num + temp1(i, 1) * temp2(i, 0) - temp1(i, 0) * temp2(i, 1);
		den = den + temp1(i, 0) * temp2(i, 0) + temp1(i, 1) * temp2(i, 1);
	}

	double norm = sqrt(num*num + den*den);
	double sin_theta = num / norm;
	double cos_theta = den / norm;

	rotation(0, 0) = cos_theta;
	rotation(0, 1) = -sin_theta;
	rotation(1, 0) = sin_theta;
	rotation(1, 1) = cos_theta;

	scale = s1 / s2;
}

cv::Mat_<double> FDUtility::RealToRelative(const cv::Mat_<double> &shape, const FDBoundingBox &boundingBox)
{
	cv::Mat_<double> temp(shape.rows, 2);
	for (int i = 0; i < shape.rows; i++)
	{
		temp(i, 0) = (shape(i, 0) - boundingBox.m_centerX) / (boundingBox.m_width / 2);
		temp(i, 1) = (shape(i, 1) - boundingBox.m_centerY) / (boundingBox.m_height / 2);
	}
	return temp;
}

cv::Mat_<double> FDUtility::RelativeToReal(const cv::Mat_<double> &shape, const FDBoundingBox &boundingBox)
{
	cv::Mat_<double> temp(shape.rows, 2);
	for (int i = 0; i < shape.rows; i++)
	{
		temp(i, 0) = shape(i, 0) * boundingBox.m_width / 2 + boundingBox.m_centerX;
		temp(i, 1) = shape(i, 1) * boundingBox.m_height / 2 + boundingBox.m_centerY;
	}
	return temp;
}

void FDUtility::GenerateTrainData(std::vector<std::string> &vecFileListPath, const std::string &cascadeClassifierModelPath, 
	int shapeGenerateNumPerSample, FDTrainData &trainData, std::vector<std::string> *pVecPath /* = NULL */)
{
	std::vector<FDTrainDataItem> srcData;
	int sz = (int)vecFileListPath.size();
	for (int i = 0; i < sz; i++)
	{
		LoadTrainData(vecFileListPath[i], cascadeClassifierModelPath, srcData, pVecPath);
	}

	std::vector<FDTrainDataItem> &generateData = trainData.mVecDataItems;
	generateData.clear();
	
	cv::RNG randomNumberGenerator(GetUInt64Value());
	int srcCount = (int)srcData.size();

	generateData.resize(srcCount * shapeGenerateNumPerSample);
	int k = 0;
	for (int i = 0; i < srcCount; i++)
	{
		FDTrainDataItem &srcItem = srcData[i];
		for (int j = 0; j < shapeGenerateNumPerSample; j++)
		{
			int index = i;
			while (index == i)
			{
				index = randomNumberGenerator.uniform(0, srcCount - 1);
			}

			FDTrainDataItem &curShapeItem = srcData[index];
			FDTrainDataItem &item = generateData[k];
			item.mImage = srcItem.mImage;
			item.mGroundTruthShape = srcItem.mGroundTruthShape;
			item.mBoundingBox = srcItem.mBoundingBox;

			item.mCurrentShape = RealToRelative(curShapeItem.mGroundTruthShape, curShapeItem.mBoundingBox);
			item.mCurrentShape = RelativeToReal(item.mCurrentShape, srcItem.mBoundingBox);
			
#ifdef SAVE_TRAIN_DATA_TO_FILE
			OutputItemInfo(item, StdStringFormat(std::string(), "%s/temp/%d.jpg", FD_TEMP_DIR, k).c_str());
#endif
			++k;
		}
	}


	CalcMeanShape(trainData);

#ifdef SAVE_TRAIN_DATA_TO_FILE
	FDTrainDataItem it = trainData.mVecDataItems[0];
	it.mCurrentShape = RelativeToReal(trainData.mMeanShape, it.mBoundingBox);
	OutputItemInfo(it, StdStringFormat(std::string(), "%s/temp/mean.jpg", FD_TEMP_DIR).c_str());
#endif
}

void FDUtility::OutputItemInfo(FDTrainDataItem &item, const char *path)
{
	cv::Mat_<uchar> dst = item.mImage.clone();
	DrawShape(item.mGroundTruthShape, dst, 0);
	DrawShape(item.mCurrentShape, dst, 255);
	cv::imwrite(path, dst);
}

void FDUtility::DrawShape(cv::Mat_<double> &shape, cv::Mat &img, unsigned char val, int radius /*= 3*/)
{
	for (int i = 0; i < shape.rows; i++)
	{
		cv::circle(img, cv::Point2d(shape(i, 0), shape(i, 1)), radius, cv::Scalar(0, 0, val), -1, 8, 0);
	}
}

bool FDUtility::LoadTrainData(const std::string &fileListPath, const std::string &cascadeClassifierModelPath, 
	std::vector<FDTrainDataItem> &vecData, std::vector<std::string> *pVecPath)
{
	std::ifstream fin;
	fin.open(fileListPath);

	cv::CascadeClassifier cascadeClassifier;
	double scale = 1.3;
	std::vector<cv::Rect> faces;
	cv::Mat gray;

	cascadeClassifier.load(cascadeClassifierModelPath);
	std::string tempPath;
	std::string ptsPath;
	int n = 0;
	std::vector<std::string> vecImgPath;
	while (std::getline(fin, tempPath))
	{
		vecImgPath.push_back(tempPath);
	}
	int imgCount = (int)vecImgPath.size();
	bool bUsed = false;
	for (int imgIndex = 0; imgIndex < imgCount; ++imgIndex)
	{
		bUsed = false;
		std::string &imgPath = vecImgPath[imgIndex];
		imgPath = Replace(imgPath, "\n", "");
		imgPath = Replace(imgPath, "\t", "");

		cv::Mat_<uchar> image = cv::imread(imgPath, cv::IMREAD_GRAYSCALE);

		ptsPath = imgPath;
		ptsPath.replace(ptsPath.find_last_of("."), 4, ".pts");
		cv::Mat_<double> groundTruthShape = LoadGroundTruthShape(ptsPath);

		cv::Mat smallImg(cvRound(image.rows / scale), cvRound(image.cols / scale), CV_8UC1);
		cv::resize(image, smallImg, smallImg.size(), 0, 0, cv::INTER_LINEAR);
		cv::equalizeHist(smallImg, smallImg);

		cascadeClassifier.detectMultiScale(smallImg, faces, 1.1, 2,
			0
			//|CV_HAAR_FIND_BIGGEST_OBJECT
			//|CV_HAAR_DO_ROUGH_SEARCH
			| CV_HAAR_SCALE_IMAGE
			,
			cv::Size(30, 30));
		int faceCount = (int)faces.size();
		for (int i = 0; i < faceCount; ++i)
		{
			cv::Rect &rect = faces[i];
			if (!IsShapeInRect(groundTruthShape, rect, scale))
				continue;

			vecData.push_back(FDTrainDataItem());
			FDTrainDataItem &item = vecData.back();
			FDBoundingBox &boundingbox = item.mBoundingBox;

			// ѵ�����ݵ�face���ܴ���ͼ����κεط�����������������
			// ����ѵ���㷨alignment��Ҫ����ֻ��face�ľֲ�ͼ�񣬶�����ȫ��ͼ��
			// ����Ӧ������cvѵ���õļ����������Զ�����������õ�������Ӿ���
			// Ȼ��������ݼ��и��������꣬�������ľֲ����꣬��Ϊѵ������
			// ��������Ԥ���������㷨������ϵ����
			// ʵ����������˹�������

			boundingbox.m_x = rect.x*scale;
			boundingbox.m_y = rect.y*scale;
			boundingbox.m_width = (rect.width - 1)*scale;
			boundingbox.m_height = (rect.height - 1)*scale;
			boundingbox.CalcCenter();


			AdjustImage(image, groundTruthShape, boundingbox);
			item.mImage = image;
			item.mGroundTruthShape = groundTruthShape;

			bUsed = true;
			break;
		}

		if (bUsed)
		{
			FDLog("%s (%d/%d)", imgPath.c_str(), imgIndex + 1, imgCount);
		}
		else
		{
			FDLog("%s (%d/%d) not match face with data", imgPath.c_str(), imgIndex + 1, imgCount);
		}
	}
	fin.close();

	if (NULL != pVecPath)
	{
		pVecPath->insert(pVecPath->end(), vecImgPath.begin(), vecImgPath.end());
	}

	return true;
}


void FDUtility::CalcMeanShape(FDTrainData &trainData)
{
	cv::Mat_<double> &meanShape = trainData.mMeanShape;
	std::vector<FDTrainDataItem> &vecItem = trainData.mVecDataItems;
	meanShape = cv::Mat::zeros(vecItem[0].mGroundTruthShape.rows, 2, CV_64FC1);
	int count = (int)vecItem.size();
	for (int i = 0; i < count; i++)
	{
		FDTrainDataItem &item = vecItem[i];
		meanShape = meanShape + RealToRelative(item.mGroundTruthShape, item.mBoundingBox);
	}
	meanShape = (1.0 / count) * meanShape;
}

void FDUtility::ShowImage(const char *windowName, const char *path)
{
	cv::Mat mat = cv::imread(path);
	cv::imshow(windowName, mat);
}

void FDUtility::Log(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf_s(fmt, args);
	va_end(args);
	std::cout << std::endl;
}

std::string& FDUtility::StdStringFormat(std::string & str, const char * fmt, ...)
{
	std::string tmp;
	va_list marker = NULL;
	va_start(marker, fmt);
	size_t num_of_chars = _vscprintf(fmt, marker);
	if (num_of_chars > tmp.capacity()) {
		tmp.resize(num_of_chars + 1);
	}
	vsprintf_s((char *)tmp.data(), tmp.capacity(), fmt, marker);
	va_end(marker);
	str = tmp.c_str();
	return str;
}

std::string FDUtility::Replace(const std::string &str, const std::string &strSrc, const std::string &strDst)
{
	if (str.empty() || strSrc.empty())
		return str;

	std::string strRes = str;
	std::string::size_type nSrcSize = strSrc.size();
	std::string::size_type nDstSize = strDst.size();
	std::string::size_type last = 0;
	std::string::size_type pos = strRes.find_first_of(strSrc, last);
	while (std::string::npos != pos)
	{
		strRes.replace(pos, nSrcSize, strDst);
		last = pos + nDstSize;
		if (last >= strRes.size())
			break;

		pos = strRes.find_first_of(strSrc, last);
	}
	return strRes;
}

cv::Mat_<double> FDUtility::LoadGroundTruthShape(const std::string &filePath)
{
	int landmarkCount = 68;
	cv::Mat_<double> shape(landmarkCount, 2);
	std::ifstream fin;
	std::string temp;

	fin.open(filePath);
	getline(fin, temp);
	getline(fin, temp);
	getline(fin, temp);
	for (int i = 0; i < landmarkCount; i++)
	{
		fin >> shape(i, 0) >> shape(i, 1);
	}
	fin.close();
	return shape;
}

void FDUtility::AdjustImage(cv::Mat_<uchar> &image, cv::Mat_<double> &ground_truth_shape, FDBoundingBox &boundingBox)
{
	double left_x = std::max(1.0, boundingBox.m_centerX - boundingBox.m_width * 2 / 3);
	double top_y = std::max(1.0, boundingBox.m_centerY - boundingBox.m_height * 2 / 3);
	double right_x = std::min(image.cols - 1.0, boundingBox.m_centerX + boundingBox.m_width);
	double bottom_y = std::min(image.rows - 1.0, boundingBox.m_centerY + boundingBox.m_height);
	image = image.rowRange((int)top_y, (int)bottom_y).colRange((int)left_x, (int)right_x).clone();

	boundingBox.m_x = boundingBox.m_x - left_x;
	boundingBox.m_y = boundingBox.m_y - top_y;
	boundingBox.CalcCenter();

	for (int i = 0; i<ground_truth_shape.rows; i++)
	{
		ground_truth_shape(i, 0) = ground_truth_shape(i, 0) - left_x;
		ground_truth_shape(i, 1) = ground_truth_shape(i, 1) - top_y;
	}
}

bool FDUtility::IsShapeInRect(const cv::Mat_<double> &shape, const cv::Rect &rect, double scale)
{
	double sum1 = 0;
	double sum2 = 0;
	double max_x = 0, min_x = 10000, max_y = 0, min_y = 10000;
	for (int i = 0; i < shape.rows; i++)
	{
		if (shape(i, 0)>max_x) max_x = shape(i, 0);
		if (shape(i, 0)<min_x) min_x = shape(i, 0);
		if (shape(i, 1)>max_y) max_y = shape(i, 1);
		if (shape(i, 1)<min_y) min_y = shape(i, 1);

		sum1 += shape(i, 0);
		sum2 += shape(i, 1);
	}
	if ((max_x - min_x)>rect.width*1.5)
		return false;

	if ((max_y - min_y)>rect.height*1.5)
		return false;

	if (abs(sum1 / shape.rows - (rect.x + rect.width / 2.0)*scale) > rect.width*scale / 2.0)
		return false;

	if (abs(sum2 / shape.rows - (rect.y + rect.height / 2.0)*scale) > rect.height*scale / 2.0)
		return false;

	return true;
}

bool FDUtility::GetShapeBoundingBox(const cv::Mat_<double> &shape, FDBoundingBox &boundingBox)
{
	if (shape.cols != 2 || shape.rows <= 0)
		return false;

	double maxX = std::numeric_limits<double>::min();
	double minX = std::numeric_limits<double>::max();
	double maxY = std::numeric_limits<double>::min();
	double minY = std::numeric_limits<double>::max();
	for (int i = 0; i < shape.rows; i++)
	{
		maxX = std::max(maxX, shape(i, 0));
		minX = std::min(minX, shape(i, 0));
		maxY = std::max(maxY, shape(i, 1));
		minY = std::min(minY, shape(i, 1));
	}

	boundingBox.m_x = minX;
	boundingBox.m_y = minY;
	boundingBox.m_width = maxX - minX;
	boundingBox.m_height = maxY - minY;
	boundingBox.CalcCenter();
	return true;
}

bool FDUtility::IsBoundingBoxInRect(const FDBoundingBox &boundingBox, const cv::Rect &rect)
{
	if (boundingBox.m_x < (double)rect.x)
		return false;

	if (boundingBox.m_y < (double)rect.y)
		return false;

	// todo ensure -1
	if (rect.x + rect.width - 1 < boundingBox.m_x + boundingBox.m_width)
		return false;

	if (rect.y + rect.height - 1 < boundingBox.m_y + boundingBox.m_height)
		return false;

	return true;
}

cv::Rect FDUtility::ScaleRect(const cv::Rect &rect, double scale)
{
	return cv::Rect((int)(rect.x * scale), (int)(rect.y * scale), (int)(rect.width * scale), (int)(rect.height * scale));
}


