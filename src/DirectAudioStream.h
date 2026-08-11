

#ifndef DIRECT_AUDIO_H
#define DIRECT_AUDIO_H

#ifdef __cplusplus
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


