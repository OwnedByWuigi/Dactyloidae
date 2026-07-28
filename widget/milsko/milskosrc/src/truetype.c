#include <Mw/Milsko.h>

#define REMAIN (size - (data - old))

MwTTFInfo MwTTFGetInfo(unsigned char* data, int size) {
	unsigned short n = 0;
	int	       i;
	unsigned char* old  = data;
	MwTTFInfo      info = malloc(sizeof(*info));

	memset(info, 0, sizeof(*info));

	if(size < 12) {
		MwTTFFreeInfo(info);
		return NULL;
	}
	n |= data[4 + 0];
	n = n << 8;
	n |= data[4 + 1];

	data += 12;
	for(i = 0; REMAIN > 0 && i < n; i++) {
		unsigned int offset = 0;
		unsigned int length = 0;
		int	     j;

		if(REMAIN < (4 + 4 + 4 + 4)) {
			MwTTFFreeInfo(info);
			return NULL;
		}

		for(j = 0; j < 4; j++) {
			offset = offset << 8;
			offset |= data[4 + 4 + j];

			length = length << 8;
			length |= data[4 + 4 + 4 + j];
		}

		if(memcmp(data, "OS/2", 4) == 0) {
			if(length >= 68) {
				unsigned char* rev	 = data;
				unsigned short us_weight = 0;

				data = old + offset;
				if(REMAIN < 68) {
					MwTTFFreeInfo(info);
					return NULL;
				}

				for(j = 0; j < 2; j++) {
					us_weight = us_weight << 8;
					us_weight |= data[0x4 + j];
				}

				info->weight = us_weight;

				data = rev;
			}
		} else if(memcmp(data, "name", 4) == 0) {
			if(length >= 6) {
				unsigned short count   = 0;
				unsigned char* rev     = data;
				unsigned short str_off = 0;
				unsigned char* beg;

				data = old + offset;
				if(REMAIN < (2 + 2 + 2)) {
					MwTTFFreeInfo(info);
					return NULL;
				}

				beg = data;

				for(j = 0; j < 2; j++) {
					count = count << 8;
					count |= data[2 + j];

					str_off = str_off << 8;
					str_off |= data[2 + 2 + j];
				}

				data += 2 + 2 + 2;
				for(j = 0; j < count; j++) {
					unsigned short platform = 0;
					unsigned short name	= 0;
					unsigned short nam_len	= 0;
					unsigned short nam_off	= 0;
					int	       k;

					if(REMAIN < (2 + 2 + 2 + 2 + 2 + 2)) {
						MwTTFFreeInfo(info);
						return NULL;
					}

					for(k = 0; k < 2; k++) {
						platform = platform << 8;
						platform |= data[0 + k];

						name = name << 8;
						name |= data[2 + 2 + 2 + k];

						nam_len = nam_len << 8;
						nam_len |= data[2 + 2 + 2 + 2 + k];

						nam_off = nam_off << 8;
						nam_off |= data[2 + 2 + 2 + 2 + 2 + k];
					}

					/* FIXME: this does not work with surrogate */
					if((platform == 0 || platform == 3) && name == 1) {
						data = beg + str_off + nam_off;
						if(REMAIN < nam_len) {
							MwTTFFreeInfo(info);
							return NULL;
						}

						if(info->family != NULL) free(info->family);

						info->family = malloc(nam_len / 2 + 1);

						for(k = 0; k < nam_len / 2; k++) info->family[k] = data[k * 2 + 1];
						info->family[k] = 0;
					}

					data += 2 + 2 + 2 + 2 + 2 + 2;
				}

				data = rev;
			}
		}

		data += 4 + 4 + 4 + 4;
	}

	return info;
}

void MwTTFFreeInfo(MwTTFInfo info) {
	if(info->family != NULL) free(info->family);
	free(info);
}
