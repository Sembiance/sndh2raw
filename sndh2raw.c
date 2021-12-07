#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <jansson.h>
#include <api68/api68.h>

#define SNDH2RAW_VERSION "1.0.0"

static void usage(void)
{
	fprintf(stderr,
			"sndh2raw %s\n"
			"\n"
			"Usage: sndh2raw <input.sndh> <outputDir>\n"
			"  -h, --help              Output this help and exit\n"
			"  -V, --version           Output version and exit\n"
			"\n", SNDH2RAW_VERSION);
	exit(EXIT_FAILURE);
}

char * inputFilePath=0;
char * outputDirPath=0;

static void parse_options(int argc, char **argv)
{
	int i;

	for(i=1;i<argc;i++)
	{
		//int lastarg = i==argc-1;

		if(!strcmp(argv[i],"-h") || !strcmp(argv[i], "--help"))
		{
			usage();
		}
		else if(!strcmp(argv[i],"-V") || !strcmp(argv[i], "--version"))
		{
			printf("sndh2raw %s\n", SNDH2RAW_VERSION);
			exit(EXIT_SUCCESS);
		}
		else
		{
			break;
		}
	}

	argc -= i;
	argv += i;

	if(argc<2)
		usage();

	inputFilePath = argv[0];
	outputDirPath = argv[1];
}

int main(int argc, char ** argv)
{
	api68_init_t init68;
	static api68_t * sc68 = 0;
	json_t * json = json_object();

	parse_options(argc, argv);

	memset(&init68, 0, sizeof(init68));
	init68.alloc = malloc;
	init68.free = free;

	sc68 = api68_init(&init68);
	if(!sc68)
		goto ERROR;

	if(api68_verify_file(inputFilePath)<0)
		goto ERROR;

	if(api68_load_file(sc68, inputFilePath))
		goto ERROR;
	
	json_object_set(json, "samplingRate", json_integer(init68.sampling_rate));

	api68_music_info_t diskInfo;
	json_t * tracksJSON=json_array();
	int trackCount=0;
	const char ** trackNames=0;
	if(!api68_music_info(sc68, &diskInfo, 0, 0))
	{
		trackCount = diskInfo.tracks;
		trackNames = (const char **)malloc(sizeof(const char *)*trackCount);
		json_object_set(json, "trackCount", json_integer(trackCount));
		for(int i=1;i<=trackCount;i++)
		{
			api68_music_info_t trackInfo;
			json_t * trackJSON = json_object();
			if(!api68_music_info(sc68, &trackInfo, i, 0))
			{
				trackNames[i-1] = trackInfo.title;
				json_object_set(trackJSON, "trackNum", json_integer(trackInfo.track));
				json_object_set(trackJSON, "title", json_string(trackInfo.title));
				json_object_set(trackJSON, "author", json_string(trackInfo.author));
				json_object_set(trackJSON, "composer", json_string(trackInfo.composer));
				json_object_set(trackJSON, "replay", json_string(trackInfo.replay));
				json_object_set(trackJSON, "type", json_string(trackInfo.hwname));
				json_object_set(trackJSON, "duration", json_integer(trackInfo.time_ms));
				json_object_set(trackJSON, "start", json_integer(trackInfo.start_ms));
				json_object_set(trackJSON, "rate", json_integer(trackInfo.rate));
			}
			json_array_append(tracksJSON, trackJSON);
		}
	}
	json_object_set(json, "tracks", tracksJSON);

	int lastTrack=0;
	char trackFilePath[2048];
	FILE * fd=0;

	if(api68_play(sc68, 1)==-1)
		goto ERROR;

	while(true)
	{
		char buffer[512 * 4];
		int code = api68_process(sc68, buffer, sizeof(buffer) >> 2);
		if(code==API68_MIX_ERROR)
			goto ERROR;
		
		if(code & API68_LOOP_BIT)
		{
			int curTrack = api68_play(sc68, -1);
			if(curTrack!=lastTrack)
			{
				if(lastTrack>0)
				{
					if(fclose(fd)!=0)
					{
						fprintf(stderr, "Failed to close output file [%s] Error: %s\n", trackFilePath, strerror(errno));
						goto ERROR;
					}
				}

				// if it's 0 then we are done
				if(curTrack==0)
					break;

				sprintf(trackFilePath, "%s/%d %s.raw", outputDirPath, curTrack, trackNames[curTrack-1]);
				fd = fopen(trackFilePath, "wb");
				if(fd==0)
				{
					fprintf(stderr, "Failed to open output file [%s] Error: %s\n", trackFilePath, strerror(errno));
					goto ERROR;
				}

				lastTrack = curTrack;
			}
		}

		if(fd==0)
		{
			fprintf(stderr, "Failed to start track, thus file wasn't opened for writing\n");
			goto ERROR;
		}

		if(fwrite(buffer, sizeof(buffer), 1, fd)!=1)
		{
			fprintf(stderr, "Failed to write %ld bytes output to [%s]\n", sizeof(buffer), trackFilePath);
			goto ERROR;
		}

		if(code & API68_END)
			break;
	}

	api68_shutdown(sc68);
	printf("%s\n", json_dumps(json, 0));
	exit(EXIT_SUCCESS);

ERROR:
	api68_shutdown(sc68);

	const char * s;
	if(s = api68_error(), s)
	{
		fprintf(stderr, "Error messages:\n");
		do
		{
			fprintf(stderr, "%s\n", s);
		} while (s = api68_error(), s!=NULL);
	}
}
