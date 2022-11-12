// System headers for any extra stuff we need.
#include <stdbool.h>

// Include CNFG (rawdraw) for generating a window and/or OpenGL context.
#define CNFG_IMPLEMENTATION
#define CNFGOGL
#include "rawdraw_sf.h"

// Include OpenVR header so we can interact with VR stuff.
#undef EXTERN_C
#include "openvr_capi.h"

// Stuff to get system time
#include <stdio.h>
#include <time.h>
#include <string.h>


// OpenVR Doesn't define these for some reason (I don't remember why) so we define the functions here. They are copy-pasted from the bottom of openvr_capi.h
intptr_t VR_InitInternal( EVRInitError *peError, EVRApplicationType eType );
void VR_ShutdownInternal();
bool VR_IsHmdPresent();
intptr_t VR_GetGenericInterface( const char *pchInterfaceVersion, EVRInitError *peError );
bool VR_IsRuntimeInstalled();
const char * VR_GetVRInitErrorAsSymbol( EVRInitError error );
const char * VR_GetVRInitErrorAsEnglishDescription( EVRInitError error );

// These are functions that rawdraw calls back into.
void HandleKey( int keycode, int bDown ) { }
void HandleButton( int x, int y, int button, int bDown ) { }
void HandleMotion( int x, int y, int mask ) { }
void HandleDestroy() { }

// This function was copy-pasted from cnovr.
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

// These are interfaces into OpenVR, they are basically function call tables.
//struct VR_IVRSystem_FnTable * oSystem;
//struct VR_IVROverlay_FnTable * oOverlay;
struct VR_IVRApplications_FnTable * oApplications;
struct VR_IVRScreenshots_FnTable * oScreenshots;
//struct VR_IVRInput_FnTable * oInput;


int main()
{
    // We put this in a codeblock because it's logically together.
	// no reason to keep the token around.
	{
		EVRInitError ierr;
		uint32_t token = VR_InitInternal( &ierr, EVRApplicationType_VRApplication_Overlay );
		if( !token )
		{
			printf( "Error!!!! Could not initialize OpenVR\n" );
			return -5;
		}

		// Get the system and overlay interfaces.  We pass in the version of these
		// interfaces that we wish to use, in case the runtime is newer, we can still
		// get the interfaces we expect.
		//oSystem = CNOVRGetOpenVRFunctionTable( IVRSystem_Version );
		//oOverlay = CNOVRGetOpenVRFunctionTable( IVROverlay_Version );
		oApplications = CNOVRGetOpenVRFunctionTable( IVRApplications_Version );
		oScreenshots = CNOVRGetOpenVRFunctionTable( IVRScreenshots_Version );
		//oInput = CNOVRGetOpenVRFunctionTable( IVRInput_Version );
	}

	if (!oApplications->IsApplicationInstalled("iigo.PISS"))
    {
		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];

		// Gets the path of the exe file
		GetModuleFileName( NULL, path_buffer, sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		_makepath( path_buffer, drive, dir, NULL, NULL ); // removes the file name and extension from the buffer
		strncat(path_buffer, "PISS.vrmanifest", sizeof("PISS.vrmanifest"));
		EVRApplicationError app_error;
		app_error = oApplications->AddApplicationManifest(path_buffer, false);

		//printf( "%s (%d).\n", path_buffer, app_error );
    }

    while( true )
    {
        printf("Press ENTER key to Continue\n");  
        getchar();

		time_t rawtime;
        struct tm * timeinfo;
		time ( &rawtime );
        timeinfo = localtime ( &rawtime );

		char path_buffer[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];


		// Gets the path of the exe file
		GetModuleFileName( NULL, path_buffer, sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		_makepath( path_buffer, drive, dir, NULL, NULL ); // removes the file name and extension from the buffer
		char ssFolderName[] = "Screenshots\\";
		strncat(path_buffer, ssFolderName, sizeof(ssFolderName));
		CreateDirectory(path_buffer, NULL);
		char ssMonthFolder[] = "0000-00\\";
		strftime(ssMonthFolder, sizeof ssMonthFolder, "%Y-%m\\", timeinfo);
		strncat(path_buffer, ssMonthFolder, sizeof ssMonthFolder);
		CreateDirectory(path_buffer, NULL);

		char timestamp[] = "2011-10-08_07-07-09";
		char screenshotpath[sizeof path_buffer + sizeof timestamp];
		strcpy(screenshotpath, path_buffer);
		strftime(timestamp, sizeof timestamp, "%Y-%m-%d_%H-%M-%S", timeinfo);
		strncat(screenshotpath, timestamp, sizeof timestamp);
		char screenshotpathvr[sizeof screenshotpath + 4];
		strcpy(screenshotpathvr, screenshotpath);
		strncat(screenshotpathvr, "_VR", 4);

		ScreenshotHandle_t screenshot;
        EVRScreenshotError ssERR;
		ssERR = oScreenshots->TakeStereoScreenshot(&screenshot, screenshotpath, screenshotpathvr);
		printf( "Screenshot (%d).\n", ssERR );
		//printf( "Current Directory: %s\n", path_buffer);

		Sleep( 50 );
    }

	return 0;
}