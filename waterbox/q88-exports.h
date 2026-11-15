/* q88-exports.h - every function the driver exports, as one table:
 * X(return type, name, (parameters), (arguments)).
 *
 * The package holds the core twice in one core.wbx (docs/memory-events.md):
 * QUASI88 as it is (the half "q88_") and QUASI88 with the calls at memory
 * (the half "q88w_"). Each half is the whole emulator and driver, with these
 * names given the half's prefix (q88-half.h) and everything else made local
 * to the half. q88-entry.c exports the real names and passes each call to
 * the half the Lua Memory Events setting chose. A new export goes here as
 * well as in q88-driver.c, or the package cannot reach it. */
#define Q88_EXPORTS(X) \
	X(const char *, GetLoadError, (void), ()) \
	X(void, SetButton, (int32_t index, int32_t state), (index, state)) \
	X(void, SetAxis, (int32_t index, int32_t value), (index, value)) \
	X(void, FrameAdvance, (uint64_t packed), (packed)) \
	X(int32_t, GetVsyncNumerator, (void), ()) \
	X(int32_t, GetVsyncDenominator, (void), ()) \
	X(uint32_t *, GetVideoBgra, (void), ()) \
	X(int, GetVideoWidth, (void), ()) \
	X(int, GetVideoHeight, (void), ()) \
	X(int16_t *, GetAudio, (void), ()) \
	X(int, GetAudioSampleCount, (void), ()) \
	X(int, InputWasRead, (void), ()) \
	X(int, GetMemoryDomainCount, (void), ()) \
	X(const char *, GetMemoryDomainName, (int i), (i)) \
	X(uint8_t *, GetMemoryDomainPtr, (int i), (i)) \
	X(int64_t, GetMemoryDomainSize, (int i), (i)) \
	X(int, GetMemoryDomainWritable, (int i), (i)) \
	X(int32_t, GetMemoryDomainHidden, (int32_t i), (i)) \
	X(int32_t, GetDriveCount, (void), ()) \
	X(const char *, GetDriveName, (int32_t i), (i)) \
	X(int32_t, GetDriveLight, (int32_t i), (i)) \
	X(int32_t, GetDriveMediaCount, (int32_t i), (i)) \
	X(const char *, GetDriveMediaName, (int32_t i, int32_t n), (i, n)) \
	X(int32_t, GetDriveMediaSelected, (int32_t i), (i)) \
	X(int32_t, GetDriveMediaInserted, (int32_t i), (i)) \
	X(int32_t, GetRegisterCount, (void), ()) \
	X(const char *, GetRegisterName, (int32_t i), (i)) \
	X(int32_t, GetRegisterBits, (int32_t i), (i)) \
	X(const char *, GetRegisterRoles, (void), ()) \
	X(int64_t, GetRegisterValue, (int32_t i), (i)) \
	X(int32_t, GetSaveDataFileCount, (void), ()) \
	X(const char *, GetSaveDataFileName, (int32_t index), (index)) \
	X(int64_t, GetSaveDataFileSize, (int32_t index), (index)) \
	X(const uint8_t *, GetSaveDataFileBuffer, (int32_t index), (index)) \
	X(const uint8_t *, GetSaveDataScratch, (void), ()) \
	X(int64_t, ReadSaveDataFile, (int32_t index, int64_t offset, int64_t len), (index, offset, len)) \
	X(uint64_t, GetFrameDigest, (void), ()) \
	X(uint64_t, GetAudioDigest, (void), ()) \
	X(uint64_t, GetEmulatedClocks, (void), ()) \
	X(uint64_t, GetFrameCount, (void), ())
/* Init is not in the table: the entry's own Init picks the half first. */

/* The calls Chimera may make before Init, when no half is chosen yet: the
 * entry gives them to both halves. X(name, (parameters), (arguments)), each
 * returning nothing. */
#define Q88_EXPORTS_BOTH(X) \
	X(SetInputPollCallback, (uint64_t fn), (fn)) \
	X(SetRenderingEnabled, (int32_t on), (on))

/* The calls at memory (Chimera issues 496, 534 and 571): only the watch
 * half (q88w_) has them; q88-entry.c answers for the plain one. */
#define Q88_EXPORTS_WATCH(X) \
	X(const char *, GetMemoryCallbackScopes, (void), ()) \
	X(int32_t, GetMemoryCallbackReplaceable, (void), ()) \
	X(const uint64_t *, GetMemoryCallbackPc, (void), ()) \
	X(void, SetMemoryCallback, (uint64_t fn), (fn)) \
	X(int32_t, AddMemoryWatch, (int32_t kind, int32_t scope, uint64_t address), (kind, scope, address)) \
	X(void, RemoveMemoryWatch, (int32_t kind, int32_t scope, uint64_t address), (kind, scope, address))
