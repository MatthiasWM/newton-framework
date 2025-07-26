

#include <filesystem>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>

extern "C" const char * GetFramesErrorString(int inErr);
extern "C" const char * GetMagicPointerString(int inMP);
extern "C" const char * StoreBackingFile(const char * inStoreName);

/* -----------------------------------------------------------------------------
	Return a string describing an error code.
	Args:		inErr		a NewtonErr code
	Return:	C string
----------------------------------------------------------------------------- */

const char *
GetFramesErrorString(int inErr)
{
#if 1
	// There seems no such table, so we just return the decimal
	static char buffer[32];
	snprintf(buffer, 32, "%d", inErr);
	return buffer;
#else
	NSString * key = [NSString stringWithFormat: @"%d", inErr];
	NSString * errStr = NSLocalizedStringFromTable(key, @"Error", NULL);
	return errStr.UTF8String;
#endif
}


/* -----------------------------------------------------------------------------
	Return a string describing a magic pointer.
	Args:		inMP		a magic pointer
	Return:	C string
----------------------------------------------------------------------------- */

const char *
GetMagicPointerString(int inMP)
{
#if 1
	// There seems no such table, so we just return the decimal, prepended with the '@'
	static char buffer[32];
	snprintf(buffer, 32, "@%d", inMP);
	return buffer;
#else
	NSString * key = [NSString stringWithFormat: @"%d", inMP];
	NSString * mpStr = NSLocalizedStringFromTable(key, @"MagicPointer", NULL);
	if (isdigit([mpStr characterAtIndex:0])) {
		mpStr = [@"@" stringByAppendingString:mpStr];
	}
	return mpStr.UTF8String;
#endif
}


/* -----------------------------------------------------------------------------
	While we’re in the Cocoa environment:
	Return the URL to the Application Support directory.
	Args:		--
	Return:	a URL
----------------------------------------------------------------------------- */

#if 1

std::filesystem::path ApplicationSupportFolder() {
#ifdef _WIN32
	auto path = std::filesystem::path(std::getenv("APPDATA")) / "Newton"; // On Windows
#elif defined(__APPLE__)
	auto path = std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "Newton"; // On macOS
#else
	auto path = std::filesystem::path(std::getenv("HOME")) / ".local" / "share" / "Newton"; // On Linux
#endif
	try {
		std::filesystem::create_directories(path);
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "Can't create application support folder " << path << ": " << e.what() << std::endl;
	} catch (const std::exception& e) {
		std::cerr << "Error creating application support folder " << path << ": " << e.what() << std::endl;
	}
	return path;
}

#else

NSURL *
ApplicationSupportFolder(void)
{
	NSFileManager * fmgr = [NSFileManager defaultManager];
	NSURL * baseURL = [fmgr URLForDirectory:NSApplicationSupportDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	NSURL * appFolder = [baseURL URLByAppendingPathComponent:@"Newton"];
	// if folder doesn’t exist, create it
	[fmgr createDirectoryAtURL:appFolder withIntermediateDirectories:NO attributes:nil error:nil];
	return appFolder;
}

#endif

/* -----------------------------------------------------------------------------
	Return the path to the store backing file.
	Args:		inStoreName		*unique* name of the store
	Return:	C string			will be autoreleased by ARC
----------------------------------------------------------------------------- */

const char *
StoreBackingFile(const char * inStoreName)
{
#if 1
	static char buffer[2048];
	auto path = ApplicationSupportFolder() / inStoreName;
	strcpy(buffer, (char*)path.c_str());
	return buffer;
#else
	NSURL * url = [ApplicationSupportFolder() URLByAppendingPathComponent:[NSString stringWithUTF8String:inStoreName]];
	return url.fileSystemRepresentation;
#endif
}
