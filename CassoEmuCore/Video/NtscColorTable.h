#pragma once

#include "Pch.h"

////////////////////////////////////////////////////////////////////////////////
//
//  NTSC Color Constants
//
//  Stored in R8G8B8A8 byte layout to match the swap-chain format
//  (DXGI_FORMAT_R8G8B8A8_UNORM in D3DRenderer.cpp). Byte 0 = R, byte 1
//  = G, byte 2 = B, byte 3 = A. With little-endian uint32_t storage
//  the hex literal therefore reads as 0xAA BB GG RR.
//
//  Pre-2026-05-13 these were written as 0xAA RR GG BB ("ARGB"
//  convention) which silently swapped Blue <-> Orange in HGR output
//  on screen because R and B got reinterpreted; Violet and Green
//  happen to be R/B-symmetric and looked correct by accident, hiding
//  the bug from any HGR content that didn't lean on the blue/orange
//  palette pair.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr uint32_t kNtscBlack   = 0xFF000000;
static constexpr uint32_t kNtscWhite   = 0xFFFFFFFF;
static constexpr uint32_t kNtscViolet  = 0xFFFD44FF;  // RGB(255,68,253)  even col, palette 0
static constexpr uint32_t kNtscGreen   = 0xFF3CF514;  // RGB(20,245,60)   odd  col, palette 0
static constexpr uint32_t kNtscBlue    = 0xFFFFCF14;  // RGB(20,207,255)  even col, palette 1
static constexpr uint32_t kNtscOrange  = 0xFF3C6AFF;  // RGB(255,106,60)  odd  col, palette 1
