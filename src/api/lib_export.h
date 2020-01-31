#pragma once

#if defined(_WIN32)
	#if defined(RVM_EXPORTS)
		#define DLL_RVM_EXPORT __declspec(dllexport)
	#else  // !BUILDING_DLL
		#define DLL_RVM_EXPORT __declspec(dllimport)
	#endif  // BUILDING_DLL
#else // / UNIX /
	#define DLL_RVM_EXPORT
#endif // 
