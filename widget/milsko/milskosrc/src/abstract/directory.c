#include <Mw/Milsko.h>

#ifdef _WIN32
typedef struct dir {
	HANDLE		hFind;
	WIN32_FIND_DATA ffd;
	int		next;
} dir_t;
#elif defined(CLASSIC_MAC_OS)
typedef struct dir {
	int dummy;
} dir_t;
#else
typedef struct dir {
	DIR*  dir;
	char* base;
} dir_t;
#endif

void* MwDirectoryOpen(const char* path) {
	dir_t* dir = malloc(sizeof(*dir));
#ifdef _WIN32
	char* p = malloc(strlen(path) + 2 + 1);
	strcpy(p, path);
	if(strchr(path, '/') != NULL) {
		strcat(p, "/");
	} else {
		strcat(p, "\\");
	}
	strcat(p, "*");
	if((dir->hFind = FindFirstFile(p, &dir->ffd)) == INVALID_HANDLE_VALUE) {
		free(p);
		free(dir);
		return NULL;
	}
	free(p);
	dir->next = 1;
#elif defined(CLASSIC_MAC_OS)
/* todo */
#else
	if((dir->dir = opendir(path)) == NULL) {
		free(dir);
		return NULL;
	}
	dir->base = malloc(strlen(path) + 1);
	strcpy(dir->base, path);
#endif

	return dir;
}

void MwDirectoryClose(void* handle) {
	dir_t* dir = handle;
#ifdef _WIN32
	FindClose(dir->hFind);
#elif defined(CLASSIC_MAC_OS)
/* todo */
#else
	closedir(dir->dir);
	free(dir->base);
#endif
	free(handle);
}

MwDirectoryEntry* MwDirectoryRead(void* handle) {
	dir_t*		  dir	= handle;
	MwDirectoryEntry* entry = malloc(sizeof(*entry));
#ifdef _WIN32
	ULARGE_INTEGER* l;

	if(!dir->next) return NULL;

	entry->name = MwStringDuplicate(dir->ffd.cFileName);
	if(dir->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		entry->type = MwDIRECTORY_DIRECTORY;
	} else {
		entry->type = MwDIRECTORY_FILE;
	}
	entry->size = 0;

	entry->size = entry->size << 32;
	entry->size |= dir->ffd.nFileSizeHigh;
	entry->size = entry->size << 32;
	entry->size |= dir->ffd.nFileSizeLow;

	l = (ULARGE_INTEGER*)&dir->ffd.ftLastWriteTime;

	entry->mtime = l->QuadPart / 10000000 - 11644473600;

	dir->next = FindNextFile(dir->hFind, &dir->ffd);
#elif defined(CLASSIC_MAC_OS)
/* todo */
#else
	struct dirent* d;
	struct stat    s;
	char*	       p;
	if((d = readdir(dir->dir)) == NULL) {
		free(entry);
		return NULL;
	}
	entry->name = MwStringDuplicate(d->d_name);

	p = malloc(strlen(dir->base) + 1 + strlen(d->d_name) + 1);
	strcpy(p, dir->base);
	strcat(p, "/");
	strcat(p, d->d_name);

	stat(p, &s);
	if(S_ISDIR(s.st_mode)) {
		entry->type = MwDIRECTORY_DIRECTORY;
	} else {
		entry->type = MwDIRECTORY_FILE;
	}
	entry->size  = s.st_size;
	entry->mtime = s.st_mtime;

	free(p);
#endif

	return entry;
}

void MwDirectoryFreeEntry(MwDirectoryEntry* entry) {
	free(entry->name);
	free(entry);
}

char* MwDirectoryCurrent(void) {
#ifdef _WIN32
	int   len = GetCurrentDirectory(0, NULL);
	char* out = malloc(len);

	GetCurrentDirectory(len, out);

	return out;
#elif CLASSIC_MAC_OS
	return "";
#else
	return getcwd(NULL, 0);
#endif
}

#ifdef _WIN32
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif
static void MwDirectoryJoinSingle(char* target, char* p) {
	int i;
	if(strcmp(p, ".") == 0) return;

	for(i = strlen(target) - 1; i >= 0; i--) {
		if(target[i] == DIRSEP) {
			target[i] = 0;
		} else {
			break;
		}
	}

	if(strcmp(p, "..") == 0) {
		for(i = strlen(target) - 1; i >= 0; i--) {
			if(target[i] != DIRSEP) {
				target[i] = 0;
			} else {
				break;
			}
		}
	} else {
		char b[2];
		b[0] = DIRSEP;
		b[1] = 0;

		strcat(target, b);
		strcat(target, p);
	}

	for(i = strlen(target) - 1; i >= 0; i--) {
		if(target[i] == DIRSEP) {
			target[i] = 0;
		} else {
			break;
		}
	}

	if(strchr(target, DIRSEP) == NULL) {
		char b[2];
		b[0] = DIRSEP;
		b[1] = 0;

		strcat(target, b);
	}
}

char* MwDirectoryJoin(char* a, char* b) {
	char* p	   = malloc(strlen(a) + 1 + strlen(b) + 1);
	char* bdup = MwStringDuplicate(b);
	char* b2   = bdup;
	int   i;

	strcpy(p, a);
	for(i = strlen(p) - 1; i >= 0; i--) {
		if(p[i] == DIRSEP) {
			p[i] = 0;
		} else {
			break;
		}
	}
	while(b2 != NULL) {
		char* current = b2;

		b2 = strchr(b2, DIRSEP);
		if(b2 != NULL) {
			b2[0] = 0;
		}

		MwDirectoryJoinSingle(p, current);

		if(b2 != NULL) b2++;
	}
	free(bdup);

	return p;
}
