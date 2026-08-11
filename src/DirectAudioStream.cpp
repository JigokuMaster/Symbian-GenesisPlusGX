
/*
 * File: DirectAudioStream.cpp
 * Initial boilerplate/template generated via Gemini.
 * Adapted, refactored, and maintained by JigokuMaster.
 */

#include "DirectAudioStream.h"
#include <e32base.h>
#include <e32std.h>
#include <stdio.h>
#include "streamplayer.h"


// Power of 2 ring buffer capacity (8192 samples = ~370ms at 22.05kHz)

#define RING_BUFFER_SIZE 8192
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)
#define DMA_FETCH_SIZE   1024



class CDirectAudioCore : public CBase, public MStreamProvider, public MStreamObs
{
public:
    static CDirectAudioCore* NewL();
    ~CDirectAudioCore();

    TInt Init(TInt aSampleRate, TInt aChannels);
    void SetVolume(TInt aVolume);
    TInt Queue(const TInt16* aSamples, TInt aCount);
    void Service();
    void Pause(TBool aPause);
    TInt GetQueuedSize() const;

private:
    CDirectAudioCore();
    void ConstructL();

    // from MStreamProvider
    TPtrC8 Data();

    // from MStreamObs
    void Complete(TInt aState, TInt aError);

private:
    CStreamPlayer* iPlayer;
    TInt16 iRingBuffer[RING_BUFFER_SIZE];
    volatile TInt iHead; // Write index
    volatile TInt iTail; // Read index

    TInt16 iDmaBuffer[DMA_FETCH_SIZE];
    TPtrC8 iDataPtr;

    TBool iIsPlaying;
    TBool iUnderflowOccurred;
    TInt  iError;
};

CDirectAudioCore::CDirectAudioCore()
    : iPlayer(NULL),
      iHead(0),
      iTail(0),
      iIsPlaying(EFalse),
      iUnderflowOccurred(EFalse),
      iError(KErrNone)
{
}

void CDirectAudioCore::ConstructL()
{

    iPlayer = new (ELeave) CStreamPlayer(*this, *this);
    iPlayer->ConstructL();
    
}

CDirectAudioCore* CDirectAudioCore::NewL()
{
    CDirectAudioCore* self = new (ELeave) CDirectAudioCore();
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
}

CDirectAudioCore::~CDirectAudioCore()
{
    if (iPlayer)
    {
        iPlayer->Stop();
        delete iPlayer;
        iPlayer = NULL;
    }
}

TInt CDirectAudioCore::Init(TInt aSampleRate, TInt aChannels)
{
    iHead = 0;
    iTail = 0;
    iUnderflowOccurred = EFalse;
    iIsPlaying = ETrue;
    TInt err = iPlayer->OpenStream(aSampleRate, aChannels, KMMFFourCCCodePCM16);
    if (err == KErrNone)
    {
        iPlayer->Start();
	CActiveScheduler::Start(); // wait for open
	err = iError;
    }
    return err;
}

void CDirectAudioCore::SetVolume(TInt aVolume)
{
    if (iPlayer) iPlayer->SetVolume(aVolume);
}


TInt CDirectAudioCore::Queue(const TInt16* aSamples, TInt aCount)
{
    // 1. Calculate current free space in ring buffer
    TInt space = (iTail - iHead - 1) & RING_BUFFER_MASK;
    if (space <= 0) return 0; // Buffer completely full

    // 2. Cap copy count to available free space
    TInt toWrite = (aCount < space) ? aCount : space;

    // 3. Determine distance to end of ring buffer memory boundary
    TInt rightChunk = RING_BUFFER_SIZE - iHead;

    if (toWrite <= rightChunk)
    {
        // Case A: Fits in a single contiguous block (No wrap-around needed)
        Mem::Copy(&iRingBuffer[iHead], aSamples, toWrite * sizeof(TInt16));
        iHead = (iHead + toWrite) & RING_BUFFER_MASK;
    }
    else
    {
        // Case B: Wraps around the end of the array (2 block copies)
        Mem::Copy(&iRingBuffer[iHead], aSamples, rightChunk * sizeof(TInt16));
        
        TInt leftChunk = toWrite - rightChunk;
        Mem::Copy(&iRingBuffer[0], aSamples + rightChunk, leftChunk * sizeof(TInt16));
        
        iHead = leftChunk;
    }

    return toWrite;
}

#if 0
TInt CDirectAudioCore::Queue(const TInt16* aSamples, TInt aCount)
{
    TInt written = 0;
    while (written < aCount)
    {
        TInt nextHead = (iHead + 1) & RING_BUFFER_MASK;
        if (nextHead == iTail)
        {
            // Ring buffer full
            break;
        }

        iRingBuffer[iHead] = aSamples[written];
        iHead = nextHead;
        written++;
    }
    return written;
}

#endif

TInt CDirectAudioCore::GetQueuedSize() const
{
    return (iHead - iTail) & RING_BUFFER_MASK;
}

// MStreamProvider: Executed when CMdaAudioOutputStream demands audio data


// DirectAudioStream.cpp - Optimized CDirectAudioCore::Data()
TPtrC8 CDirectAudioCore::Data()
{
    TInt available = GetQueuedSize();

    if (!iIsPlaying || available == 0)
    {
        iDataPtr.Set(KNullDesC8());
        return iDataPtr;
    }

    // Limit chunk size to DMA fetch size
    TInt fetchCount = (available > DMA_FETCH_SIZE) ? DMA_FETCH_SIZE : available;

    // Direct memory descriptor pointing into iRingBuffer without per-sample loops
    // Handles linear read up to buffer wrap boundary
    TInt contig = RING_BUFFER_SIZE - iTail;
    TInt sendCount = (fetchCount < contig) ? fetchCount : contig;

    iDataPtr.Set(reinterpret_cast<const TUint8*>(&iRingBuffer[iTail]), sendCount * sizeof(TInt16));
    iTail = (iTail + sendCount) & RING_BUFFER_MASK;

    return iDataPtr;
}

#if 0
TPtrC8 CDirectAudioCore::Data()
{
    TInt available = GetQueuedSize();

    if (!iIsPlaying || available == 0)
    {
        iDataPtr.Set(KNullDesC8());
        return iDataPtr;
    }

    // Limit chunk size to DMA fetch size
    TInt fetchCount = (available > DMA_FETCH_SIZE) ? DMA_FETCH_SIZE : available;

    for (TInt i = 0; i < fetchCount; ++i)
    {
        iDmaBuffer[i] = iRingBuffer[iTail];
        iTail = (iTail + 1) & RING_BUFFER_MASK;
    }

    iDataPtr.Set(reinterpret_cast<const TUint8*>(iDmaBuffer), fetchCount * static_cast<TInt>(sizeof(TInt16)));
    return iDataPtr;
}

#endif

void CDirectAudioCore::Complete(TInt aState, TInt aError)
{
    iError = aError;
    if (aState == MStreamObs::EInit)
    {
	CActiveScheduler::Stop();
    }

    if (aError == KErrUnderflow || aError == KErrInUse)
    {
        iUnderflowOccurred = ETrue;
    }
    else if (aError == KErrAbort)
    {
        iPlayer->Open();
    }
}

void CDirectAudioCore::Service()
{
    // Auto-restart stream on underflow if buffer has built back up
    if (iUnderflowOccurred && iIsPlaying)
    {
        if (GetQueuedSize() >= DMA_FETCH_SIZE)
        {
            iUnderflowOccurred = EFalse;
            iPlayer->Start();
        }
    }
}

void CDirectAudioCore::Pause(TBool aPause)
{
    iIsPlaying = !aPause;
    if (aPause)
    {
        iPlayer->Stop();
    }
    else
    {
        iPlayer->Start();
    }
}

// -------------------------------------------------------------------
// C API Functions (Global Instance Wrapper)
// -------------------------------------------------------------------
static CDirectAudioCore* gAudioCore = NULL;

extern "C" int DirectAudio_Init(int sampleRate, int channels)
{
    if (gAudioCore)
    {
        DirectAudio_Quit();
    }

    TRAPD(err, gAudioCore = CDirectAudioCore::NewL());
    if (err != KErrNone)
    {
        return err;
    }

    return gAudioCore->Init(sampleRate, channels);
}


extern "C" void DirectAudio_SetVolume(int volume)
{
    if (gAudioCore)
    {
	gAudioCore->SetVolume(volume);
    }
}


extern "C" void DirectAudio_Quit(void)
{
    if (gAudioCore)
    {
        delete gAudioCore;
        gAudioCore = NULL;
    }
}

extern "C" int DirectAudio_Queue(const short* samples, int count)
{
    if (!gAudioCore) return 0;
    return gAudioCore->Queue(reinterpret_cast<const TInt16*>(samples), count);
}

extern "C" void DirectAudio_Service(void)
{
    if (gAudioCore)
    {
        gAudioCore->Service();
    }
}

extern "C" void DirectAudio_Pause(int pauseOn)
{
    if (gAudioCore)
    {
        gAudioCore->Pause(pauseOn ? ETrue : EFalse);
    }
}

extern "C" int DirectAudio_GetQueuedSize(void)
{
    if (!gAudioCore) return 0;
    return gAudioCore->GetQueuedSize();
}

