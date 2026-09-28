/*
 File: EmbeddedApp.h

 A Newton app as a macOS app of its own: newtc with a package compiled in
 (CMake: -DNEWTC_APP_PKG=<app.pkg>, target newtc_app; cmake/EmbedPackage.cmake
 writes the data). Built so (NEWTC_EMBEDDED_APP), newtc started without
 arguments (a double click on the app) runs the package as

   newtc -store "~/Library/Application Support/<name>/<name>.store"
         -pkg "~/Library/Application Support/<name>/<name>.pkg" -run

 writing the package there first: its soups are kept between runs, in the
 user's own folder. With arguments, it is newtc as always.
 */

#ifndef MATT_EMBEDDEDAPP_H
#define MATT_EMBEDDEDAPP_H

#include <cstddef>

/** The app's name (the bundle's, and its folder's in Application Support). */
extern const char gEmbeddedAppName[];

/** The package's bytes. */
extern const unsigned char gEmbeddedAppPackage[];
extern const size_t gEmbeddedAppPackageSize;

#endif // MATT_EMBEDDEDAPP_H
