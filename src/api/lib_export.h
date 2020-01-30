#pragma once

#ifndef __linux__
#if defined(PMUC_EXPORTS)
#define DLL_PMUC_EXPORT __declspec(dllexport)
#else  // !BUILDING_DLL
#define DLL_PMUC_EXPORT __declspec(dllimport)
#endif  // BUILDING_DLL
#else
#define DLL_pmuc_EXPORT
#endif  // __linux__