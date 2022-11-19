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

		}

		Sleep( 500 ); /* sleep for half a second (500ms) */
    }

	return 0;
}