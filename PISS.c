/*
 * Periodic Immersive SteamVR Screenshots is a simple utility that utilizes the OpenVR api to take a
 * SteamVR screenshot every hour on the top of the hour.
 */

/* System headers for any extra stuff we need. */
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

/* 
 * Include CNFG (rawdraw) for generating a window and/or OpenGL context.
 * Included to manage windows header files, but may be used more explicitly in the future.
 */
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

/* Include OpenVR header so we can interact with VR stuff. */
#undef EXTERN_C
#include "openvr_capi.h"

/* Include the configuration */
#include "config.h"

/*
 * OpenVR Doesn't define these for some reason (I don't remember why) so we
 * define the functions here. They are copy-pasted from the bottom of openvr_capi.
 */
intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
bool VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
bool VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

/* These are functions that rawdraw calls back into. */
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

/* This function was copy-pasted from cnovr. */
void * CNOVRGetOpenVRFunctionTable( const char * interfacename )
{
	EVRInitError e;
	char fnTableName[128];
	int result1 = snprintf( fnTableName, 128, "FnTable:%s", interfacename );
	void * ret = (void *)VR_GetGenericInterface( fnTableName, &e );
	printf( "Getting System FnTable: %s = %p (%d)\n", fnTableName, ret, e );
	if( !ret )
	{
		exit( 1 );
	}
	return ret;
}

/* These are interfaces into OpenVR, they are basically function call tables. */
//struct VR_IVRSystem_FnTable * oSystem;
//struct VR_IVROverlay_FnTable * oOverlay;
struct VR_IVRApplications_FnTable * oApplications;
struct VR_IVRScreenshots_FnTable * oScreenshots;
//struct VR_IVRInput_FnTable * oInput;


static void
takescreenshot(struct tm * tm_struct, char * ssFilepath)
{
	char path_buffer[_MAX_PATH];
	strcpy(path_buffer, ssFilepath);
	char ssFolderName[] = "Screenshots\\";
	strncat(path_buffer, ssFolderName, sizeof(ssFolderName));
	CreateDirectory(path_buffer, NULL);
	char ssMonthFolder[] = "1970-01\\";
	strftime(ssMonthFolder, sizeof(ssMonthFolder), "%Y-%m\\", tm_struct);
	strncat(path_buffer, ssMonthFolder, sizeof(ssMonthFolder));
	CreateDirectory(path_buffer, NULL);

	char timestamp[] = "1970-01-01_00-00-00";
	char screenshotpath[sizeof(path_buffer) + sizeof(timestamp)];
	strcpy(screenshotpath, path_buffer);
	strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_struct);
	strncat(screenshotpath, timestamp, sizeof(timestamp));
	char screenshotpathvr[sizeof(screenshotpath) + 4];
	strcpy(screenshotpathvr, screenshotpath);
	strncat(screenshotpathvr, "_VR", 4);

	ScreenshotHandle_t screenshot;
	EVRScreenshotError ssERR;
	EVRScreenshotType ssType = EVRScreenshotType_VRScreenshotType_Stereo;

	/* applys save stereo screenshot setting set in config.h */
	if (savestereosscreenshot == 0) { ssType = EVRScreenshotType_VRScreenshotType_Mono; }

	ssERR = oScreenshots->RequestScreenshot(&screenshot, ssType, screenshotpath, screenshotpathvr);

	printf( "Screenshot (%d).\n", ssERR );
	printf( "Current Directory: %s\n", screenshotpath);
}

#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define QR(a, b, c, d) (			\
	a += b,  d ^= a,  d = ROTL(d,16),	\
	c += d,  b ^= c,  b = ROTL(b,12),	\
	a += b,  d ^= a,  d = ROTL(d, 8),	\
	c += d,  b ^= c,  b = ROTL(b, 7))
#define ROUNDS 20
 
static void 
chacha_block(uint32_t out[16], uint32_t const in[16])
{
	int i;
	uint32_t x[16];

	for (i = 0; i < 16; ++i)	
		x[i] = in[i];
	// 10 loops × 2 rounds/loop = 20 rounds
	for (i = 0; i < ROUNDS; i += 2) {
		// Odd round
		QR(x[0], x[4], x[ 8], x[12]); // column 0
		QR(x[1], x[5], x[ 9], x[13]); // column 1
		QR(x[2], x[6], x[10], x[14]); // column 2
		QR(x[3], x[7], x[11], x[15]); // column 3
		// Even round
		QR(x[0], x[5], x[10], x[15]); // diagonal 1 (main diagonal)
		QR(x[1], x[6], x[11], x[12]); // diagonal 2
		QR(x[2], x[7], x[ 8], x[13]); // diagonal 3
		QR(x[3], x[4], x[ 9], x[14]); // diagonal 4
	}
	for (i = 0; i < 16; ++i)
		out[i] = x[i] + in[i];
}

/* 
 * Takes the current time and gets the amount of hours since unix epoch and then
 * uses that with a ChaCha20 block to create a psudo-random block, and then the
 * first uint32_t of that block is then mod 3000 to get a random value in seconds
 * that is between 0 and 50 minutes, this value is then added with 300 which is 
 * 5 minutes in seconds which is then added to the value of the current hour in
 * seconds giving us a timestamp between the start of the hour + (00:05 - 00:55)
 * if the current time is greater then the time chosen then it will return max value
 */

/* returns a random time within the hour between the middle 50 minutes of the hour (XX:05 - XX:55)*/
static time_t
PRNGTimeGen(time_t const currentTime)
{
	uint32_t chachainit[16];
	/* setup the chacha20 constants */
	chachainit[0] = 0x61707865U; /* "expa" */
    chachainit[1] = 0x3320646eU; /* "nd 3" */
    chachainit[2] = 0x79622d32U; /* "2-by" */
    chachainit[3] = 0x6b206574U; /* "te k" */
	/* explicity set the value of the "key"*/
	chachainit[4]  = 0x00000000U;
	chachainit[5]  = 0x00000000U;
	chachainit[6]  = 0x00000000U;
	chachainit[7]  = 0x00000000U;
	chachainit[8]  = 0x00000000U;
	chachainit[9]  = 0x00000000U;
	chachainit[10] = 0x00000000U;
	chachainit[11] = 0x00000000U;
	/* explicity set the value of the "counter" */
	chachainit[12] = 0x00000000U;
	/* explicity set the value of the "nonce" */
	chachainit[13] = 0x00000000U;
	/* last two positions are used filled by the number of hours since unix epoch */
	uint64_t hoursSinceEpoch;
	hoursSinceEpoch = currentTime / 3600;
	chachainit[14] = ((hoursSinceEpoch & 0xFFFFFFFF00000000LL) >> 32);
	chachainit[15] = (hoursSinceEpoch & 0xFFFFFFFFLL);
	uint32_t chachaout[16];
	chacha_block(chachaout, chachainit);
	time_t PRNGTime = 0;
	uint32_t addedSeconds = 0;
	addedSeconds = (chachaout[0] % 3000) + 300;
	PRNGTime = (hoursSinceEpoch * 3600) + addedSeconds;
	/* we do this so if you start the program after the chosen time it does not attempt to screenshot */
	if (PRNGTime <= currentTime)
	{
		printf("screenshot time for this hour has already passed\n");
		return (time_t)(hoursSinceEpoch * 3600) + (3600 * 24);
	}
	return PRNGTime;
}

char ssFilepath[_MAX_PATH];

int main()
{
    /* 
	 * We put this in a codeblock because it's logically together. 
	 * no reason to keep the token around.
	 */
	{
		EVRInitError ierr;
		uint32_t token = VR_InitInternal( &ierr, EVRApplicationType_VRApplication_Overlay );
		if( !token )
		{
			printf( "Error!!!! Could not initialize OpenVR\n" );
			return -5;
		}

		/* 
		 * Get the system and overlay interfaces.  We pass in the version of these
		 * interfaces that we wish to use, in case the runtime is newer, we can still
		 * get the interfaces we expect.
		 */
		//oSystem = CNOVRGetOpenVRFunctionTable( IVRSystem_Version );
		//oOverlay = CNOVRGetOpenVRFunctionTable( IVROverlay_Version );
		oApplications = CNOVRGetOpenVRFunctionTable( IVRApplications_Version );
		oScreenshots = CNOVRGetOpenVRFunctionTable( IVRScreenshots_Version );
		//oInput = CNOVRGetOpenVRFunctionTable( IVRInput_Version );
	}

	/* Ensure that the application is registered in SteamVR so you can setup autolaunch */
	{
		if (!oApplications->IsApplicationInstalled("iigo.PISS"))
		{
			char path_buffer[_MAX_PATH];
			strcpy(path_buffer, ssFilepath);
			strncat(path_buffer, "PISS.vrmanifest", sizeof("PISS.vrmanifest"));
			EVRApplicationError app_error;
			app_error = oApplications->AddApplicationManifest(path_buffer, false);

			//printf( "%s (%d).\n", path_buffer, app_error );
		}
	}

	/* Setup where the screenshots will be saved to */
	{
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];

		/* Gets the path of the exe file */
		GetModuleFileName( NULL, path_buffer, sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		_makepath( path_buffer, drive, dir, NULL, NULL ); /* removes the file name and extension from the buffer */
		strcpy(ssFilepath, path_buffer);
	}

	time_t now = time(NULL);
	//time_t now = 1667707200; /* 2022 Nov 6th at midnight */
	//time_t now = 1678597200; /* 2023 Mar 12th at midnight */
	int hoursSinceEpoch = now / 3600;
	int sshour = hoursSinceEpoch;

	printf("initialized successfully\n");

	time_t PRNGTime = PRNGTimeGen(now);
	//char PRNGtimestamp[] = "1970-01-01_00-00-00";
	//struct tm *PRNGTime_struct = gmtime(&PRNGTime);
	//strftime(PRNGtimestamp, sizeof(PRNGtimestamp), "%Y-%m-%d_%H-%M-%S", PRNGTime_struct);
	//printf("%s", PRNGtimestamp);


    while( true )
    {

		//now = now + 1; /* increase time by one second each while loop */
		now = time(NULL);
		
		hoursSinceEpoch = now / 3600;
		struct tm *tm_struct = gmtime(&now);

		/* check to see if its a new hour */
		if (sshour != hoursSinceEpoch)
		{
			if (takehourlyscreenshot != 0)
			{
				takescreenshot(tm_struct, ssFilepath);
			}

			sshour = hoursSinceEpoch;

			PRNGTime = PRNGTimeGen(now);

		}

		/* check to see if its a new hour */
		if (now >= PRNGTime)
		{
			if (takerandomscreenshot != 0)
			{
				takescreenshot(tm_struct, ssFilepath);
				char PRNGtimestamp[] = "1970-01-01_00-00-00";
				struct tm *PRNGTime_struct = gmtime(&PRNGTime);
				strftime(PRNGtimestamp, sizeof(PRNGtimestamp), "%Y-%m-%d_%H-%M-%S", PRNGTime_struct);
				printf("Random screenshot at (%s) \n", PRNGtimestamp);
			}

			PRNGTime = (time_t)(hoursSinceEpoch * 3600) + (3600 * 24);

		}



		Sleep(500); /* sleep for half a second (500ms) */
    }

	return 0;
}