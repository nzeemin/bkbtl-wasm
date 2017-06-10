/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include <stdio.h>
#include "emubase\Emubase.h"
#include <emscripten/emscripten.h>


bool Emulator_LoadRomFile(LPCTSTR strFileName, uint8_t* buffer, uint32_t fileOffset, uint32_t bytesToRead);

void Emulator_PrepareScreenRGB32(void* pImageBits);
void CALLBACK Emulator_PrepareScreenColor512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits);


const uint32_t ScreenView_ColorPalette[4] =
{
    0xFF000000, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF
};


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;
BKConfiguration g_nEmulatorConfiguration;  // Current configuration

bool g_okEmulatorInitialized = false;
bool g_okEmulatorRunning = false;

uint16_t m_wEmulatorCPUBreakpoint = 0177777;

bool m_okEmulatorSound = false;

void* g_pFrameBuffer = 0;

long m_nFrameCount = 0;
uint32_t m_dwTickCount = 0;
uint32_t m_dwEmulatorUptime = 0;  // BK uptime, seconds, from turn on or reset, increments every 25 frames
long m_nUptimeFrameCount = 0;


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_BKROM_MONIT10    = _T("monit10.rom");
const LPCTSTR FILENAME_BKROM_FOCAL      = _T("focal.rom");
const LPCTSTR FILENAME_BKROM_TESTS      = _T("tests.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_1  = _T("basic10_1.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_2  = _T("basic10_2.rom");
const LPCTSTR FILENAME_BKROM_BASIC10_3  = _T("basic10_3.rom");
const LPCTSTR FILENAME_BKROM_DISK_326   = _T("disk_326.rom");
const LPCTSTR FILENAME_BKROM_BK11M_BOS  = _T("b11m_bos.rom");
const LPCTSTR FILENAME_BKROM_BK11M_EXT  = _T("b11m_ext.rom");
const LPCTSTR FILENAME_BKROM_BASIC11M_0 = _T("basic11m_0.rom");
const LPCTSTR FILENAME_BKROM_BASIC11M_1 = _T("basic11m_1.rom");
const LPCTSTR FILENAME_BKROM_BK11M_MSTD = _T("b11m_mstd.rom");


//////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
extern "C" {
#endif

void EMSCRIPTEN_KEEPALIVE Emulator_Init()
{
    printf("Emulator_Init()\n");

    g_pFrameBuffer = malloc(640 * 288 * 4);

    ASSERT(g_pBoard == NULL);

    CProcessor::Init();

    g_pBoard = new CMotherboard();

    g_pBoard->Reset();

    //g_okEmulatorAutoTapeReading = false;
    //g_pEmulatorAutoTapeReadingFilename = NULL;

    g_okEmulatorInitialized = true;
}

void EMSCRIPTEN_KEEPALIVE Emulator_InitConfiguration(uint16_t configuration)
{
    printf("Emulator_InitConfiguration(%d)\n", configuration);

    g_pBoard->SetConfiguration(configuration);

    uint8_t buffer[8192];

    if ((configuration & BK_COPT_BK0011) == 0)
    {
        // Load Monitor ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_MONIT10, buffer, 0, 8192))
        {
            printf("Failed to load Monitor ROM file.");
            return;
        }
        g_pBoard->LoadROM(0, buffer);
    }

    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_BASIC) != 0 ||
        (configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_FDD) != 0)  // BK 0010 BASIC ROM 1-2
    {
        // Load BASIC ROM 1 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_1, buffer, 0, 8192))
        {
            printf(_T("Failed to load BASIC ROM 1 file."));
            return;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BASIC ROM 2 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_2, buffer, 0, 8192))
        {
            printf(_T("Failed to load BASIC ROM 2 file."));
            return;
        }
        g_pBoard->LoadROM(2, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_BASIC) != 0)  // BK 0010 BASIC ROM 3
    {
        // Load BASIC ROM 3 file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC10_3, buffer, 0, 8064))
        {
            printf(_T("Failed to load BASIC ROM 3 file."));
            return;
        }
        g_pBoard->LoadROM(3, buffer);
    }
    if ((configuration & BK_COPT_BK0011) == 0 && (configuration & BK_COPT_ROM_FOCAL) != 0)  // BK 0010 FOCAL
    {
        // Load Focal ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_FOCAL, buffer, 0, 8192))
        {
            printf(_T("Failed to load Focal ROM file."));
            return;
        }
        g_pBoard->LoadROM(1, buffer);
        // Unused 8KB
        ::memset(buffer, 0, 8192);
        g_pBoard->LoadROM(2, buffer);
        // Load Tests ROM file
        if (!Emulator_LoadRomFile(FILENAME_BKROM_TESTS, buffer, 0, 8064))
        {
            printf(_T("Failed to load Tests ROM file."));
            return;
        }
        g_pBoard->LoadROM(3, buffer);
    }

    if (configuration & BK_COPT_BK0011)
    {
        // Load BK0011M BASIC 0, part 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 0, 8192))
        {
            printf(_T("Failed to load BK11M BASIC 0 ROM file."));
            return;
        }
        g_pBoard->LoadROM(0, buffer);
        // Load BK0011M BASIC 0, part 2
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_0, buffer, 8192, 8192))
        {
            printf(_T("Failed to load BK11M BASIC 0 ROM file."));
            return;
        }
        g_pBoard->LoadROM(1, buffer);
        // Load BK0011M BASIC 1
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BASIC11M_1, buffer, 0, 8192))
        {
            printf(_T("Failed to load BK11M BASIC 1 ROM file."));
            return;
        }
        g_pBoard->LoadROM(2, buffer);

        // Load BK0011M EXT
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_EXT, buffer, 0, 8192))
        {
            printf(_T("Failed to load BK11M EXT ROM file."));
            return;
        }
        g_pBoard->LoadROM(3, buffer);
        // Load BK0011M BOS
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_BOS, buffer, 0, 8192))
        {
            printf(_T("Failed to load BK11M BOS ROM file."));
            return;
        }
        g_pBoard->LoadROM(4, buffer);
    }

    if (configuration & BK_COPT_FDD)
    {
        // Load disk driver ROM file
        ::memset(buffer, 0, 8192);
        if (!Emulator_LoadRomFile(FILENAME_BKROM_DISK_326, buffer, 0, 4096))
        {
            printf(_T("Failed to load DISK ROM file."));
            return;
        }
        g_pBoard->LoadROM((configuration & BK_COPT_BK0011) ? 5 : 3, buffer);
    }

    if ((configuration & BK_COPT_BK0011) && (configuration & BK_COPT_FDD) == 0)
    {
        // Load BK0011M MSTD
        if (!Emulator_LoadRomFile(FILENAME_BKROM_BK11M_MSTD, buffer, 0, 8192))
        {
            printf(_T("Failed to load BK11M MSTD ROM file."));
            return;
        }
        g_pBoard->LoadROM(5, buffer);
    }

    g_nEmulatorConfiguration = (BKConfiguration)configuration;

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwEmulatorUptime = 0;

}

uint32_t EMSCRIPTEN_KEEPALIVE Emulator_GetUptime()
{
    return m_dwEmulatorUptime;
}

uint16_t EMSCRIPTEN_KEEPALIVE Emulator_GetReg()
{
    return g_pBoard->GetCPU()->GetPC();
}

void EMSCRIPTEN_KEEPALIVE Emulator_Start()
{
    printf("Emulator_Start()\n");

    g_okEmulatorRunning = true;
}
void EMSCRIPTEN_KEEPALIVE Emulator_Stop()
{
    printf("Emulator_Stop()\n");

    g_okEmulatorRunning = false;
}

void EMSCRIPTEN_KEEPALIVE Emulator_Reset()
{
    printf("Emulator_Reset()\n");

    ASSERT(g_pBoard != NULL);

    g_pBoard->Reset();
}

void EMSCRIPTEN_KEEPALIVE Emulator_SystemFrame()
{
    //printf("Emulator_SystemFrame()\n");

    //SoundGen_SetVolume(Settings_GetSoundVolume());

    g_pBoard->SetCPUBreakpoint(m_wEmulatorCPUBreakpoint);

    //ScreenView_ScanKeyboard();

    if (!g_pBoard->SystemFrame())
        return;

    // Calculate emulator uptime (25 frames per second)
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= 25)
    {
        m_dwEmulatorUptime++;
        m_nUptimeFrameCount = 0;
    }
}

void* EMSCRIPTEN_KEEPALIVE Emulator_PrepareScreen()
{
    //printf("Emulator_PrepareScreen()\n");
    if (g_pFrameBuffer == 0)
        printf("Emulator_PrepareScreen() null framebuffer\n");

    Emulator_PrepareScreenRGB32(g_pFrameBuffer);

    return g_pFrameBuffer;
}

void EMSCRIPTEN_KEEPALIVE Emulator_KeyEvent(uint32_t scancode)
{
    //TODO
}

#ifdef __cplusplus
}
#endif

void Emulator_PrepareScreenRGB32(void* pImageBits)
{
    if (pImageBits == NULL) return;

    // Get scroll value
    uint16_t scroll = g_pBoard->GetPortView(0177664);
    bool okSmallScreen = ((scroll & 01000) == 0);
    scroll &= 0377;
    scroll = (scroll >= 0330) ? scroll - 0330 : 050 + scroll;

    //const uint32_t * pPalette = Emulator_GetPalette(screenMode);
    const uint32_t * pPalette = ScreenView_ColorPalette;

    const uint8_t* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != NULL);

    // Render to bitmap
    //PREPARE_SCREEN_CALLBACK callback = ScreenModeReference[screenMode].callback;
    //callback(pVideoBuffer, okSmallScreen, pPalette, scroll, pImageBits);
    Emulator_PrepareScreenColor512x256(pVideoBuffer, okSmallScreen, pPalette, scroll, pImageBits);
}

bool Emulator_LoadRomFile(LPCTSTR strFileName, uint8_t* buffer, uint32_t fileOffset, uint32_t bytesToRead)
{
    FILE* fpRomFile = ::fopen(strFileName, _T("rb"));
    if (fpRomFile == NULL)
        return false;

    ASSERT(bytesToRead <= 8192);
    ::memset(buffer, 0, 8192);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    size_t dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return false;
    }

    ::fclose(fpRomFile);

    return true;
}

void CALLBACK Emulator_PrepareScreenColor512x256(const uint8_t* pVideoBuffer, int okSmallScreen, const uint32_t* pPalette, int scroll, void* pImageBits)
{
    int linesToShow = okSmallScreen ? 64 : 256;
    for (int y = 0; y < linesToShow; y++)
    {
        int yy = (y + scroll) & 0377;
        const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + yy * 0100);
        uint32_t* pBits = (uint32_t*)pImageBits + y * 512;
        for (int x = 0; x < 512 / 16; x++)
        {
            uint16_t src = *pVideo;

            for (int bit = 0; bit < 16; bit += 2)
            {
                uint32_t color = pPalette[src & 3];
                *pBits = color;
                pBits++;
                *pBits = color;
                pBits++;
                src = src >> 2;
            }

            pVideo++;
        }
    }
    if (okSmallScreen)
    {
        memset((uint32_t*)pImageBits, 0, (256 - 64) * 512 * sizeof(uint32_t));
    }
}


//////////////////////////////////////////////////////////////////////

int main()
{
    Emulator_Init();
    Emulator_InitConfiguration(2);
    Emulator_Start();
}
