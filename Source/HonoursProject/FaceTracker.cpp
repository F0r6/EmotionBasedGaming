#include "FaceTracker.h"


AFaceTracker::AFaceTracker()
{
    PrimaryActorTick.bCanEverTick = true;

    // Set default cascade paths
    HaarCascadePath = FPaths::ProjectContentDir() + TEXT("HaarCascades/haarcascade_frontalface_default.xml");
    EyeCascadePath = FPaths::ProjectContentDir() + TEXT("HaarCascades/haarcascade_eye.xml");
    SmileCascadePath = FPaths::ProjectContentDir() + TEXT("HaarCascades/haarcascade_smile.xml");
    
    VideoWidth = 640;
    VideoHeight = 480;
    VideoUpdateTextureRegion = nullptr;
    ProcessingThread = nullptr;
    Thread = nullptr;
    TimeSinceLastUpdate = 0.0f;
    LastDetectedEmotion = EFacialEmotion::Neutral;
     

}

// Called when the game starts or when spawned
void AFaceTracker::BeginPlay()
{
	Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("Initializing Facial Expression Tracker..."));
    
    // Initialize webcam
    VideoCapture.open(0);
    
    if (!VideoCapture.isOpened())
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to open webcam"));
        return;
    }
    
    // Set webcam resolution
    VideoCapture.set(cv::CAP_PROP_FRAME_WIDTH, VideoWidth);
    VideoCapture.set(cv::CAP_PROP_FRAME_HEIGHT, VideoHeight);
    VideoCapture.set(cv::CAP_PROP_FPS, TargetFPS);
    VideoCapture.set(cv::CAP_PROP_BUFFERSIZE, 1); // Minimize buffering
    
    // Get actual resolution
    VideoWidth = VideoCapture.get(cv::CAP_PROP_FRAME_WIDTH);
    VideoHeight = VideoCapture.get(cv::CAP_PROP_FRAME_HEIGHT);
    
    UE_LOG(LogTemp, Log, TEXT("Webcam opened: %dx%d @ %d FPS"), VideoWidth, VideoHeight, TargetFPS);
    
    // Load Haar Cascades
    std::string FaceCascadePathStr(TCHAR_TO_UTF8(*HaarCascadePath));
    std::string EyeCascadePathStr(TCHAR_TO_UTF8(*EyeCascadePath));
    std::string SmileCascadePathStr(TCHAR_TO_UTF8(*SmileCascadePath));
    
    if (!FaceCascade.load(FaceCascadePathStr))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load Face Cascade from: %s"), *HaarCascadePath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Face Cascade loaded successfully"));
    }
    
    if (!EyeCascade.load(EyeCascadePathStr))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load Eye Cascade from: %s"), *EyeCascadePath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Eye Cascade loaded successfully"));
    }
    
    if (!SmileCascade.load(SmileCascadePathStr))
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load Smile Cascade from: %s"), *SmileCascadePath);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("Smile Cascade loaded successfully"));
    }
    
    // Create texture
    VideoTexture = UTexture2D::CreateTransient(VideoWidth, VideoHeight, PF_B8G8R8A8);
    if (VideoTexture)
    {
        VideoTexture->UpdateResource();
        UE_LOG(LogTemp, Log, TEXT("Video texture created successfully"));
    }
    
    // Create update region
    VideoUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, VideoWidth, VideoHeight);
    
    // Initialize buffer
    VideoBuffer.SetNum(VideoWidth * VideoHeight * 4);
    
    // Start processing thread
    ProcessingThread = new FVideoProcessingThread(&VideoCapture, &FaceCascade, &EyeCascade, &SmileCascade);
    Thread = FRunnableThread::Create(ProcessingThread, TEXT("VideoProcessingThread"), 0, TPri_Normal);
    
    UE_LOG(LogTemp, Log, TEXT("Facial tracking initialized with threading and emotion detection"));
    
}

// Called every frame
void AFaceTracker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    
    if (!ProcessingThread || !VideoTexture)
    {
        return;
    }
    
    TimeSinceLastUpdate += DeltaTime;
    
    // Limit texture update rate
    float UpdateInterval = 1.0f / TargetFPS;
    if (TimeSinceLastUpdate < UpdateInterval)
    {
        return;
    }
    
    TimeSinceLastUpdate = 0.0f;
    
    // Get processed frame from worker thread
    cv::Mat ProcessedFrame;
    if (ProcessingThread->GetProcessedFrame(ProcessedFrame))
    {
        UpdateTexture(ProcessedFrame);
    }
    
    // Get emotion data
    DetectedEmotions = ProcessingThread->GetEmotionData();
    
    // Trigger Blueprint event if emotion changed
    if (DetectedEmotions.Num() > 0)
    {
        EFacialEmotion CurrentEmotion = DetectedEmotions[0].Emotion;
        if (CurrentEmotion != LastDetectedEmotion)
        {
            LastDetectedEmotion = CurrentEmotion;
            OnEmotionDetected(CurrentEmotion, DetectedEmotions[0].Confidence);
        }
    }
    
}

void AFaceTracker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
        
    UE_LOG(LogTemp, Log, TEXT("Shutting down Facial Expression Tracker..."));
    
    // Stop thread
    if (ProcessingThread)
    {
        ProcessingThread->Stop();
    }
    
    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
    
    if (ProcessingThread)
    {
        delete ProcessingThread;
        ProcessingThread = nullptr;
    }
    
    // Release webcam
    if (VideoCapture.isOpened())
    {
        VideoCapture.release();
        UE_LOG(LogTemp, Log, TEXT("Webcam released"));
    }
    
    // Clean up texture region
    if (VideoUpdateTextureRegion)
    {
        delete VideoUpdateTextureRegion;
        VideoUpdateTextureRegion = nullptr;
    }
    
    UE_LOG(LogTemp, Log, TEXT("Facial Expression Tracker shutdown complete"));
    
}

void AFaceTracker::UpdateTexture(cv::Mat& Frame)
{
    if (!VideoTexture || Frame.empty())
    {
        return;
    }
    
    // Convert BGR to BGRA
    cv::Mat FrameBGRA;
    cv::cvtColor(Frame, FrameBGRA, cv::COLOR_BGR2BGRA);
    
    // Copy to buffer
    FMemory::Memcpy(VideoBuffer.GetData(), FrameBGRA.data, VideoWidth * VideoHeight * 4);
    
    // Update texture on game thread
    VideoTexture->UpdateTextureRegions(
        0,
        1,
        VideoUpdateTextureRegion,
        VideoWidth * 4,
        4,
        VideoBuffer.GetData()
    );
}

FVideoProcessingThread::FVideoProcessingThread(cv::VideoCapture* InCapture, cv::CascadeClassifier* InFaceCascade,
	cv::CascadeClassifier* InEyeCascade, cv::CascadeClassifier* InSmileCascade)
: VideoCapture(InCapture)
, FaceCascade(InFaceCascade)
, EyeCascade(InEyeCascade)
, SmileCascade(InSmileCascade)
, bRunning(true)
{
}

FVideoProcessingThread::~FVideoProcessingThread()
{
	Stop();
}

bool FVideoProcessingThread::Init()
{
	UE_LOG(LogTemp, Log, TEXT("Video processing thread initialized"));
	return true;
}

uint32 FVideoProcessingThread::Run()
{
	while (bRunning)
	{
		if (VideoCapture && VideoCapture->isOpened())
		{
		    ProcessFrame();
		}

	    if (GEngine)
	    {
	        GEngine->AddOnScreenDebugMessage(
            -1,
            .12f,
            FColor::Cyan,
            FString::Printf(TEXT("EyeApectRatio: %f"), EyeAspectRatio));

	        GEngine->AddOnScreenDebugMessage(
            -1,
            .12f,
            FColor::Blue,
            FString::Printf(TEXT("RelativeEyeSize: %f"), RelativeEyeSize));

	        GEngine->AddOnScreenDebugMessage(
            -1,
            .12f,
            FColor::Green,
            FString::Printf(TEXT("SmileIntensity: %f"), SmileIntensity));
    
	    }
	    
		// Control frame rate (30 FPS = ~33ms per frame)
		FPlatformProcess::Sleep(0.033f);
	}
    
	return 0;
}

void FVideoProcessingThread::Stop()
{
    bRunning = false;
}

void FVideoProcessingThread::Exit()
{
    UE_LOG(LogTemp, Log, TEXT("Video processing thread exiting"));
}

bool FVideoProcessingThread::GetProcessedFrame(cv::Mat& OutFrame)
{
    FScopeLock Lock(&FrameMutex);
    if (!ProcessedFrame.empty())
    {
        OutFrame = ProcessedFrame.clone();
        return true;
    }
    return false;
}

TArray<FFacialEmotionData> FVideoProcessingThread::GetEmotionData()
{
    FScopeLock Lock(&EmotionMutex);
    return EmotionResults;
}

void FVideoProcessingThread::ProcessFrame()
{
	cv::Mat Frame;
    
    // Capture frame
    if (!VideoCapture->read(Frame))
    {
        return;
    }
    
    if (Frame.empty())
    {
        return;
    }
    
    // Flip for mirror effect
    cv::flip(Frame, Frame, 1);
    
    // Convert to grayscale
    cv::Mat GrayFrame, SmallFrame;
    cv::cvtColor(Frame, GrayFrame, cv::COLOR_BGR2GRAY);
    
    // Resize for faster processing
    cv::resize(GrayFrame, SmallFrame, cv::Size(), 0.5, 0.5);
    cv::equalizeHist(SmallFrame, SmallFrame);
    
    // Detect faces
    std::vector<cv::Rect> Faces;
    FaceCascade->detectMultiScale(SmallFrame, Faces, 1.1, 3, 0, cv::Size(20, 20));
    
    TArray<FFacialEmotionData> NewEmotions;
    
    for (size_t i = 0; i < Faces.size(); i++)
    {
        // Scale back to original size
        cv::Rect ScaledFace(Faces[i].x * 2, Faces[i].y * 2, 
                           Faces[i].width * 2, Faces[i].height * 2);
        
        // Ensure face rect is within image bounds
        ScaledFace.x = FMath::Max(0, ScaledFace.x);
        ScaledFace.y = FMath::Max(0, ScaledFace.y);
        ScaledFace.width = FMath::Min(ScaledFace.width, GrayFrame.cols - ScaledFace.x);
        ScaledFace.height = FMath::Min(ScaledFace.height, GrayFrame.rows - ScaledFace.y);
        
        if (ScaledFace.width <= 0 || ScaledFace.height <= 0)
        {
            continue;
        }
        
        // Get face region from original grayscale
        cv::Mat FaceROI = GrayFrame(ScaledFace);
        
        // Detect emotion
        float Confidence = 0.0f;
        EFacialEmotion Emotion = DetectEmotion(FaceROI, ScaledFace, Confidence);
        
        // Create emotion data
        FFacialEmotionData EmotionData;
        EmotionData.Emotion = Emotion;
        EmotionData.Confidence = Confidence;
        EmotionData.FaceCenter = FVector2D(
            ScaledFace.x + ScaledFace.width / 2.0f,
            ScaledFace.y + ScaledFace.height / 2.0f
        );
        EmotionData.FaceSize = ScaledFace.width;
        NewEmotions.Add(EmotionData);
        
        // Draw on frame
        cv::Scalar Color;
        std::string EmotionText;
        
        switch(Emotion)
        {
            case EFacialEmotion::Happy:
                Color = cv::Scalar(0, 255, 0);
                EmotionText = "Happy";
                break;
            case EFacialEmotion::Sad:
                Color = cv::Scalar(255, 0, 0);
                EmotionText = "Sad";
                break;
            case EFacialEmotion::Angry:
                Color = cv::Scalar(0, 0, 255);
                EmotionText = "Angry";
                break;
            case EFacialEmotion::Surprised:
                Color = cv::Scalar(255, 255, 0);
                EmotionText = "Surprised";
                break;
            case EFacialEmotion::Fearful:
                Color = cv::Scalar(255, 0, 255);
                EmotionText = "Fearful";
                break;
            case EFacialEmotion::Disgusted:
                Color = cv::Scalar(128, 0, 128);
                EmotionText = "Disgusted";
                break;
            default:
                Color = cv::Scalar(128, 128, 128);
                EmotionText = "Neutral";
                break;
        }
        
        // Draw rectangle around face
        cv::rectangle(Frame, ScaledFace, Color, 3);
        
        // Draw emotion label
        cv::putText(Frame, EmotionText, 
                   cv::Point(ScaledFace.x, ScaledFace.y - 10),
                   cv::FONT_HERSHEY_SIMPLEX, 0.9, Color, 2);
        
        // Draw confidence
        std::string ConfidenceText = "Conf: " + std::to_string((int)(Confidence * 100)) + "%";
        cv::putText(Frame, ConfidenceText,
                   cv::Point(ScaledFace.x, ScaledFace.y + ScaledFace.height + 25),
                   cv::FONT_HERSHEY_SIMPLEX, 0.6, Color, 2);
    }
    
    // Update emotion results thread-safely
    {
        FScopeLock Lock(&EmotionMutex);
        EmotionResults = NewEmotions;
    }
    
    // Update processed frame thread-safely
    {
        FScopeLock Lock(&FrameMutex);
        ProcessedFrame = Frame.clone();
    }
}

EFacialEmotion FVideoProcessingThread::DetectEmotion(const cv::Mat& FaceROI, const cv::Rect& FaceRect, float& OutConfidence)
{
	// Detect eyes in the face region
    std::vector<cv::Rect> Eyes;
    EyeCascade->detectMultiScale(FaceROI, Eyes, 1.1, 3, 0, cv::Size(15, 15));
    
    // Detect smile in the lower half of face
    cv::Rect LowerFaceRect(0, FaceROI.rows / 2, FaceROI.cols, FaceROI.rows / 2);
    cv::Mat LowerFaceROI = FaceROI(LowerFaceRect);
    
    std::vector<cv::Rect> Smiles;
    SmileCascade->detectMultiScale(LowerFaceROI, Smiles, 1.8, 20, 0, cv::Size(25, 25));
    
    // Calculate features
    HasSmile = Smiles.size() > 0;
    HasBothEyes = Eyes.size() >= 2;
    HasOneEye = Eyes.size() == 1;
    HasNoEyes = Eyes.size() == 0;
    
    // Calculate eye characteristics
    float AvgEyeHeight = 0.0f;
    float AvgEyeWidth = 0.0f;
    
    if (Eyes.size() > 0)
    {
        for (const auto& Eye : Eyes)
        {
            AvgEyeHeight += Eye.height;
            AvgEyeWidth += Eye.width;
        }
        AvgEyeHeight /= Eyes.size();
        AvgEyeWidth /= Eyes.size();
    }
    
    // Eye aspect ratio (height/width) - wider eyes have higher ratio
    EyeAspectRatio = AvgEyeWidth > 0 ? AvgEyeHeight / AvgEyeWidth : 0.0f;
    
    // Relative eye size compared to face
    RelativeEyeSize = FaceRect.height > 0 ? AvgEyeHeight / FaceRect.height : 0.0f;
    
    // Smile characteristics
    SmileWidth = 0.0f;
    SmileHeight = 0.0f;
    
    if (HasSmile)
    {
        for (const auto& Smile : Smiles)
        {
            SmileWidth += Smile.width;
            SmileHeight += Smile.height;
        }
        SmileWidth /= Smiles.size();
        SmileHeight /= Smiles.size();
    }
    
    SmileIntensity = Smiles.size();
    
    // Emotion classification logic
    EFacialEmotion DetectedEmotion = EFacialEmotion::Neutral;
    OutConfidence = 0.5f; // Base confidence
    
    // Happy: Has smile and normal/wide eyes
    if (HasSmile && SmileIntensity >= 1)
    {
        DetectedEmotion = EFacialEmotion::Happy;
        OutConfidence = FMath::Clamp(0.6f + (SmileIntensity * 0.1f), 0.0f, 1.0f);
    }
    // Surprised: Wide eyes (high aspect ratio), possibly no smile
    else if (HasBothEyes && RelativeEyeSize > 0.15f)
    {
        DetectedEmotion = EFacialEmotion::Surprised;
        OutConfidence = FMath::Clamp(0.55f + (RelativeEyeSize * 2.0f), 0.0f, 1.0f);
    }
    // Angry: Squinted eyes or no eyes detected (eyes closed/narrowed), no smile
    else if ((HasNoEyes || HasOneEye) && !HasSmile)
    {
        DetectedEmotion = EFacialEmotion::Angry;
        OutConfidence = 0.55f;
    }
    // Sad: Eyes detected but small, no smile
    else if (HasBothEyes && !HasSmile && RelativeEyeSize < 0.12f)
    {
        DetectedEmotion = EFacialEmotion::Sad;
        OutConfidence = 0.5f;
    }
    // Fearful: Similar to surprised but with different eye characteristics
    else if (HasBothEyes && RelativeEyeSize > 0.13f && !HasSmile)
    {
        DetectedEmotion = EFacialEmotion::Fearful;
        OutConfidence = 0.5f;
    }
    // Neutral: Default state
    else
    {
        DetectedEmotion = EFacialEmotion::Neutral;
        OutConfidence = 0.6f;
    }
    
    return DetectedEmotion;
}


