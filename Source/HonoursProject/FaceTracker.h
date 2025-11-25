// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "OpenCVHelper.h"
#include "Engine/Texture2D.h"

#include "MediaCapture.h"
#include "IMediaEventSink.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

#include "PreOpenCVHeaders.h"
#include "opencv2/opencv.hpp"
#include "opencv2/dnn.hpp"
#include "opencv2/objdetect.hpp"
#include "PostOpenCVHeaders.h"

#include "FaceTracker.generated.h"


UENUM(BlueprintType)
enum class EFacialEmotion : uint8
{
	Neutral  	UMETA(DisplayName = "Neutral"),
	Happy    	UMETA(DisplayName = "Happy"),
	Sad      	UMETA(DisplayName = "Sad"),
	Angry    	UMETA(DisplayName = "Angry"),
	Surprised   UMETA(DisplayName = "Surprised"),
	Fearful  	UMETA(DisplayName = "Fearful"),
	Disgusted   UMETA(DisplayName = "Disgusted")
};


USTRUCT(BlueprintType)
struct FFacialEmotionData
{
	GENERATED_BODY()
    
	UPROPERTY(BlueprintReadOnly)
	EFacialEmotion Emotion = EFacialEmotion::Neutral;
    
	UPROPERTY(BlueprintReadOnly)
	float Confidence = 0.0f;
    
	UPROPERTY(BlueprintReadOnly)
	FVector2D FaceCenter = FVector2D::ZeroVector;
    
	UPROPERTY(BlueprintReadOnly)
	float FaceSize = 0.0f;
	
};



//USTRUCT(BlueprintType)
//struct FEmotionDetectionSettings
//{
//	GENERATED_BODY()
//	
//	// Happy detection
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Happy")
//	float HappySmileIntensityThreshold = 1.0f;
//	
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Happy")
//	float HappySmileWidthRatio = 0.3f;
//	
//	// Surprised detection
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surprised")
//	float SurprisedEyeSizeThreshold = 0.15f;
//	
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surprised")
//	float SurprisedEyePositionThreshold = 0.35f;
//	
//	// Angry detection
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angry")
//	float AngryEyePositionThreshold = 0.4f;
//	
//	// Sad detection
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sad")
//	float SadEyeSizeThreshold = 0.13f;
//	
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sad")
//	float SadMouthBrightnessThreshold = 0.6f;
//	
//	// Fearful detection
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fearful")
//	float FearfulEyeSizeMin = 0.13f;
//	
//	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fearful")
//	float FearfulEyeSizeMax = 0.17f;
//};
//

// Worker thread class

class FVideoProcessingThread : public FRunnable
{
public:
	FVideoProcessingThread(cv::VideoCapture* InCapture, 
						  cv::CascadeClassifier* InFaceCascade,
						  cv::CascadeClassifier* InEyeCascade,
						  cv::CascadeClassifier* InSmileCascade);
	virtual ~FVideoProcessingThread();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	// Get processed frame
	bool GetProcessedFrame(cv::Mat& OutFrame);
    
	// Get emotion data
	TArray<FFacialEmotionData> GetEmotionData();
	
private:
	cv::VideoCapture* VideoCapture;
	cv::CascadeClassifier* FaceCascade;
	cv::CascadeClassifier* EyeCascade;
	cv::CascadeClassifier* SmileCascade;
    
	cv::Mat CurrentFrame;
	cv::Mat ProcessedFrame;
    
	FCriticalSection FrameMutex;
	FCriticalSection EmotionMutex;
	FThreadSafeBool bRunning;
    
	TArray<FFacialEmotionData> EmotionResults;

	bool HasSmile;
	bool HasBothEyes;
	bool HasOneEye;
	bool HasNoEyes;

	float SmileWidth = 0.0f;
	float SmileHeight = 0.0f;
	
	float EyeAspectRatio;
	float RelativeEyeSize;
	float SmileIntensity;
	float BrowIntensity;
	float MouthAspectRatio = 0.0f;
	
	TArray<EFacialEmotion> EmotionHistory;
	const int HistorySize = 10;
	
	void ProcessFrame();
	EFacialEmotion DetectEmotion(const cv::Mat& FaceROI, const cv::Rect& FaceRect, float& OutConfidence);
};
 


UCLASS()
class HONOURSPROJECT_API AFaceTracker : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AFaceTracker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Facial Tracking")
	void OnEmotionDetected(EFacialEmotion Emotion, float Confidence);
    
	UFUNCTION(BlueprintCallable, Category = "Facial Tracking")
	UTexture2D* GetVideoTexture() const { return VideoTexture; }

	UFUNCTION(BlueprintCallable, Category = "Face Tracking")
	TArray<FFacialEmotionData> GetDetectedEmotions() const { return DetectedEmotions; }

	
	UPROPERTY(BlueprintReadOnly, Category = "Facial Tracking")
	UTexture2D* VideoTexture;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Tracking")
	int32 VideoWidth;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial Tracking")
	int32 VideoHeight;
    
	UPROPERTY(BlueprintReadOnly, Category = "Facial Tracking")
	TArray<FFacialEmotionData> DetectedEmotions;
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	int32 TargetFPS = 30;
    
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	//float DetectionScale = 0.5f;
    
	// Cascade paths
	UPROPERTY(EditAnywhere, Category = "Facial Tracking")
	FString HaarCascadePath;
    
	UPROPERTY(EditAnywhere, Category = "Facial Tracking")
	FString EyeCascadePath;
    
	UPROPERTY(EditAnywhere, Category = "Facial Tracking")
	FString SmileCascadePath;
	
	UPROPERTY(BlueprintReadOnly, Category = "Facial Tracking")
	EFacialEmotion LastDetectedEmotion;

private:
	cv::VideoCapture VideoCapture;
	cv::CascadeClassifier FaceCascade;
	cv::CascadeClassifier EyeCascade;
	cv::CascadeClassifier SmileCascade;
    
	void UpdateTexture(cv::Mat& Frame);
    
	FUpdateTextureRegion2D* VideoUpdateTextureRegion;
	TArray<uint8> VideoBuffer;
    
	// Threading
	FVideoProcessingThread* ProcessingThread;
	FRunnableThread* Thread;
    
	float TimeSinceLastUpdate;
	
};


