

#ifndef DIRECT_AUDIO_H
#define DIRECT_AUDIO_H

#ifdef __cplusplus
#include <mdaaudiooutputstream.h>
#include <mda/common/audio.h>

class CSymbianAudioStream : public CBase, public MMdaAudioOutputStreamCallback
{
public:
    static CSymbianAudioStream* NewL(TInt aSampleRate, TInt aBufferSize);
    ~CSymbianAudioStream();
    TBool WriteData(TUint8* aData, TInt aLen);
    TBool DirectWriteData(TUint8* aData, TInt aLen);
    TBool UpdateSndRate();
    void SetVolume(TInt aNewVolume);

    // from MMdaAudioOutputStreamCallback
    virtual void MaoscOpenComplete(TInt aError);
    virtual void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer);
    virtual void MaoscPlayComplete(TInt /*aError*/);

private:
    CSymbianAudioStream(TInt aSampleRate, TInt aBufferSize);
    void ConstructL();
    TBool                   iIsOpen;
    int                     iSampleRate;
    CMdaAudioOutputStream*  iStream;
    TMdaAudioDataSettings   iSettings;
    TInt                    iVolume;
    TInt 		    iError;
    RBuf8	    	    iBuffer;
    TInt 		    iBufferSize;
    TPtrC8	    	    iDataPtr;
};


extern "C" {
#endif

/**
 * Initializes the direct audio engine.
 * @param sampleRate Frequency in Hz (e.g., 16000, 22050, 44100)
 * @param channels 1 for Mono, 2 for Stereo
 * @return 0 on success, non-zero Symbian error code on failure.
 */
int DirectAudio_Init(int sampleRate, int channels);

void DirectAudio_SetVolume(int volume);

/**
 * Stops playback and cleans up all audio resources.
 */
void DirectAudio_Quit(void);

/**
 * Pushes 16-bit PCM samples into the audio ring buffer.
 * @param samples Pointer to 16-bit PCM buffer
 * @param count Number of samples (not bytes) to queue
 * @return Number of samples successfully queued
 */
int DirectAudio_Queue(const short* samples, int count);

/**
 * Pumps Symbian active signals and recovers from stream underflows.
 * Must be called once per frame inside your main event loop.
 */
void DirectAudio_Service(void);

/**
 * Pauses or resumes audio output.
 * @param pauseOn 1 to pause, 0 to play
 */
void DirectAudio_Pause(int pauseOn);

/**
 * Returns the number of unplayed audio samples currently in the buffer.
 */
int DirectAudio_GetQueuedSize(void);

#ifdef __cplusplus
}
#endif

#endif // DIRECT_AUDIO_H


