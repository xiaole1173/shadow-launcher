// Copyright (C) 2025-2026 影 / Shadow / xiaole1173
// Minimal WebView2 login window — based on the official WebView2APISample
// build configuration (NuGet packages, /MT, stdcpp17) which is verified
// working on this machine.
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wrl/event.h>
#include <WebView2.h>

#include <string>
