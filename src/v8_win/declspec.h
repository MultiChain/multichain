#ifndef DECLSPEC_H
#define DECLSPEC_H

#ifdef WIN32
#ifdef V8_WIN_EXPORTS
   /*Enabled as "export" while compiling the dll project*/
   #define DLLEXPORT __declspec(dllexport)
#else
   /*Enabled as "import" in the Client side for using already created dll file*/
   #define DLLEXPORT __declspec(dllimport)
#endif
#else
   #define DLLEXPORT
#endif // WIN32

#endif // DECLSPEC_H
