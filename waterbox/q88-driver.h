/* q88-driver.h - the driver's limits and the calls the harnesses make. */
#ifndef Q88_DRIVER_H
#define Q88_DRIVER_H

#include <stdint.h>

/* The largest picture: QUASI88's full-size screen. */
#define Q88DRV_VIDEO_MAX_W 640
#define Q88DRV_VIDEO_MAX_H 400
/* Sound pairs one frame can hold: the 800 of a frame, with room. */
#define Q88DRV_AUDIO_MAX_PAIRS 4096

const char *GetLoadError(void);
int Init(void);
void FrameAdvance(uint64_t packed);
int32_t GetVsyncNumerator(void);
int32_t GetVsyncDenominator(void);
uint32_t *GetVideoBgra(void);
int GetVideoWidth(void);
int GetVideoHeight(void);
int16_t *GetAudio(void);
int GetAudioSampleCount(void);
int GetMemoryDomainCount(void);
const char *GetMemoryDomainName(int i);
uint8_t *GetMemoryDomainPtr(int i);
int64_t GetMemoryDomainSize(int i);
uint64_t GetFrameDigest(void);
uint64_t GetEmulatedClocks(void);
uint64_t GetAudioDigest(void);
void SetButton(int32_t index, int32_t state);
void SetAxis(int32_t index, int32_t value);
int InputWasRead(void);
int32_t GetSaveDataFileCount(void);
const char *GetSaveDataFileName(int32_t index);
int64_t GetSaveDataFileSize(int32_t index);
const uint8_t *GetSaveDataScratch(void);
int64_t ReadSaveDataFile(int32_t index, int64_t offset, int64_t len);
void SetRenderingEnabled(int32_t on);
int32_t GetDriveCount(void);
int32_t GetDriveLight(int32_t i);
int32_t GetRegisterCount(void);
const char *GetRegisterName(int32_t i);
int64_t GetRegisterValue(int32_t i);
void SetInputPollCallback(uint64_t fn);
const char *GetMemoryCallbackScopes(void);
int32_t GetMemoryCallbackReplaceable(void);
void SetMemoryCallback(uint64_t fn);
int32_t AddMemoryWatch(int32_t kind, int32_t scope, uint64_t address);
void RemoveMemoryWatch(int32_t kind, int32_t scope, uint64_t address);

#endif
