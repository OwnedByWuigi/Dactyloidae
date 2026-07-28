#include <Mw/Milsko.h>

#if defined(_WIN32)
void* MwDynamicOpen(const char* path) {
	return LoadLibrary(path);
}

void* MwDynamicSymbol(void* handle, const char* symbol) {
	return GetProcAddress(handle, symbol);
}

void MwDynamicClose(void* handle) {
	FreeLibrary(handle);
}
#elif defined(CLASSIC_MAC_OS)
void* MwDynamicOpen(const char* path) {
	// CFragConnectionID connID;
	// Ptr*		  addr;
	// unsigned char	  err[255];

	// if(GetSharedLibrary((ConstStr63Param)path, kPowerPCCFragArch, kPrivateCFragCopy,
	// 		    &connID, addr, err)) {
	// 	printf("Error getting %s: %s\n", path, err);
	// 	return NULL;
	// };
	// return connID;
	printf("MwDynamicOpen not implemented yet.\n");
	getchar();
	return NULL;
}

void* MwDynamicSymbol(void* handle, const char* symbol) {
	// CFragConnectionID connID = (CFragConnectionID)handle;
	// Ptr*		  symAddr;
	// CFragSymbolClass* symClass;
	// OSErr		  err;
	// if((err = FindSymbol(connID, (ConstStr63Param)symbol, symAddr, symClass))) {
	// 	printf("Error getting %s: %d\n", symbol, err);
	// 	return NULL;
	// };
	// return symAddr;
	printf("MwDynamicSymbol not implemented yet.\n");
	getchar();
	return NULL;
}

void MwDynamicClose(void* handle) {
	// CFragConnectionID connID = (CFragConnectionID)handle;
	// CloseConnection(&connID);
}
#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)
void* MwDynamicOpen(const char* path) {
	return dlopen(path, RTLD_LOCAL | RTLD_LAZY);
}

void* MwDynamicSymbol(void* handle, const char* symbol) {
	return dlsym(handle, symbol);
}

void MwDynamicClose(void* handle) {
	dlclose(handle);
}
#endif
