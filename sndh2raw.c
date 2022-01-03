#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <jansson.h>
#include <sc68/sc68.h>

#define SNDH2RAW_VERSION "1.1.0"

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

char * strchrtrim(char * str, char letter)
{
    if(!str)
        return str;

    while(*str==letter && *str)
	{
        strcpy(str, str+1);
	}

	return str;
}

char * strrchrtrim(char * str, char letter)
{
    if(!str)
       	return str;

    while(str[(strlen(str)-1)] == letter && strlen(str))
	{
        str[(strlen(str)-1)] = '\0';
	}

    return str;
}


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
	static sc68_t * sc68 = 0;
	sc68_create_t create68;
	json_t * json = json_object();

	parse_options(argc, argv);

	if(sc68_init(0))
		goto ERROR;

	memset(&create68, 0, sizeof(create68));
	sc68 = sc68_create(&create68);
	if(sc68==0)
		goto ERROR;

	if(sc68_load_uri(sc68, inputFilePath))
		goto ERROR;
	
	json_object_set(json, "samplingRate", json_integer(create68.sampling_rate));

	sc68_music_info_t diskInfo;
	json_t * tracksJSON=json_array();
	int trackCount=0;
	char ** trackNames=0;
	char * albumTitle;
	if(!sc68_music_info(sc68, &diskInfo, 0, 0))
	{
		trackCount = diskInfo.tracks;
		trackNames = (char **)malloc(sizeof(char *)*trackCount);
		json_object_set(json, "trackCount", json_integer(trackCount));
		albumTitle = strchrtrim(strrchrtrim(strdup(diskInfo.title), ' '), ' ');
		json_object_set(json, "albumTitle", json_string(albumTitle));
		for(int i=1;i<=trackCount;i++)
		{
			sc68_music_info_t trackInfo;
			json_t * trackJSON = json_object();
			if(!sc68_music_info(sc68, &trackInfo, i, 0))
			{
				trackNames[i-1] = strchrtrim(strrchrtrim(strdup(trackInfo.title), ' '), ' ');
				json_object_set(trackJSON, "trackNum", json_integer(trackInfo.trk.track));
				json_object_set(trackJSON, "title", json_string(trackNames[i-1]));
				json_object_set(trackJSON, "artist", json_string(trackInfo.artist));
				json_object_set(trackJSON, "replay", json_string(trackInfo.replay));
				json_object_set(trackJSON, "type", json_string(trackInfo.trk.hw));
				json_object_set(trackJSON, "duration", json_integer(trackInfo.trk.time_ms));
				json_object_set(trackJSON, "rate", json_integer(trackInfo.rate));
			}
			json_array_append(tracksJSON, trackJSON);
		}
	}
	json_object_set(json, "tracks", tracksJSON);

	int lastTrack=0;
	int curTrack=1;
	char trackFilePath[2048];
	FILE * fd=0;

	if(sc68_play(sc68, curTrack, 0)==-1)
		goto ERROR;
		
	while(true)
	{
		char buffer[512 * 4];
		int numSamples = sizeof(buffer) >> 2;
		int code = sc68_process(sc68, buffer, &numSamples);
		if(code==SC68_ERROR)
			goto ERROR;

		if(code & SC68_CHANGE)
		{
			if(lastTrack>0)
			{
				if(fclose(fd)!=0)
				{
					fprintf(stderr, "Failed to close output file [%s] Error: %s\n", trackFilePath, strerror(errno));
					goto ERROR;
				}				
			}

			lastTrack = curTrack;
			curTrack = sc68_play(sc68, SC68_CUR_TRACK, 0);

			sprintf(trackFilePath, "%s/%s %02d - %s.raw", outputDirPath, albumTitle, curTrack, trackNames[curTrack-1]);
			fd = fopen(trackFilePath, "wb");
			if(fd==0)
			{
				fprintf(stderr, "Failed to open output file [%s] Error: %s\n", trackFilePath, strerror(errno));
				goto ERROR;
			}

			//printf("change flag %d => %d\n", lastTrack, curTrack);
		}

		if(fd==0)
		{
			fprintf(stderr, "Failed to start track, thus file wasn't opened for writing\n");
			goto ERROR;
		}

		//if(code & SC68_LOOP)
		//	printf("loop flag\n");

		if(code & SC68_END)
		{
			//printf("end flag\n");

			if(fclose(fd)!=0)
			{
				fprintf(stderr, "Failed to close output file [%s] Error: %s\n", trackFilePath, strerror(errno));
				goto ERROR;
			}

			break;
		}


		//printf("write buffer\n");
		if(fwrite(buffer, sizeof(buffer), 1, fd)!=1)
		{
			fprintf(stderr, "Failed to write %ld bytes output to [%s]\n", sizeof(buffer), trackFilePath);
			goto ERROR;
		}
	}

	sc68_shutdown();
	printf("%s\n", json_dumps(json, 0));
	exit(EXIT_SUCCESS);

ERROR:
	sc68_shutdown();

	fprintf(stderr, "Errors:\n");
	const char * s;
	s = sc68_error(sc68);
	if(s && strlen(s))
		fprintf(stderr, "%s\n", s);
	
	while(true)
	{
		s = sc68_error(0);
		if(!s || !strlen(s))
			break;
		
		fprintf(stderr, "%s\n", s);
	}
}
