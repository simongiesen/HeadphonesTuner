// HeadphonesTuner.cpp
// Simon Giesen, Sensitec GmbH, 2026-06-15
// L/R-Balance (Kanal-Lautstaerke) des Standard-Wiedergabegeraets setzen
// ueber die Core Audio EndpointVolume API - funktioniert oft auch dann,
// wenn der Balance-Regler in den Windows-Einstellungen ausgegraut ist,
// weil hier eine SOFTWARE-Kanallautstaerke auf den Stereostream gelegt wird.
//
// Kompilieren:
//   MSVC:  cl /EHsc balance.cpp Ole32.lib
//   MinGW: g++ HeadphonesTuner.cpp -o HeadphonesTuner.exe -lole32 -luuid
//
// Aufruf:
//   HeadphonesTuner.exe          -> zeigt Geraet, Kanalzahl und aktuelle Pegel
//   HeadphonesTuner.exe 100 80   -> links 100 %, rechts 80 %

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include <cstdlib>

#define CHECK(hr, msg) \
    if (FAILED(hr)) { printf("Fehler: %s (hr=0x%08lx)\n", msg, (unsigned long)(hr)); goto cleanup; }

static float clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

int main(int argc, char** argv) {
    HRESULT hr;
    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioEndpointVolume* pVol = nullptr;
    IPropertyStore* pProps = nullptr;
    UINT channels = 0;

    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) { printf("CoInitializeEx fehlgeschlagen\n"); return 1; }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
    CHECK(hr, "MMDeviceEnumerator erzeugen");

    // Standard-Wiedergabegeraet. Zum Testen das USB-Headset als
    // Standardgeraet setzen, oder hier auf EnumAudioEndpoints() umbauen,
    // um gezielt ein bestimmtes Geraet zu waehlen.
    hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    CHECK(hr, "Standard-Wiedergabegeraet holen");

    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT name; PropVariantInit(&name);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &name)) && name.vt == VT_LPWSTR)
            wprintf(L"Geraet : %s\n", name.pwszVal);
        PropVariantClear(&name);
    }

    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
        nullptr, (void**)&pVol);
    CHECK(hr, "EndpointVolume aktivieren");

    hr = pVol->GetChannelCount(&channels);
    CHECK(hr, "GetChannelCount");
    printf("Kanaele: %u\n", channels);

    for (UINT i = 0; i < channels; ++i) {
        float lvl = 0.f;
        pVol->GetChannelVolumeLevelScalar(i, &lvl);
        printf("  Kanal %u (%s): %.0f %%\n",
            i, i == 0 ? "L" : (i == 1 ? "R" : "?"), lvl * 100.f);
    }

    if (argc >= 3) {
        if (channels < 2) {
            printf("\nGeraet meldet nur %u Kanal - keine Kanal-Balance ueber diese API moeglich.\n",
                channels);
            printf("=> App-Steuerung nicht möglich.\n");
        }
        else {
            float left = clamp01((float)atof(argv[1]) / 100.f);
            float right = clamp01((float)atof(argv[2]) / 100.f);
            hr = pVol->SetChannelVolumeLevelScalar(0, left, nullptr);
            CHECK(hr, "Kanal L setzen");
            hr = pVol->SetChannelVolumeLevelScalar(1, right, nullptr);
            CHECK(hr, "Kanal R setzen");
            printf("\nGesetzt: L = %.0f %%  R = %.0f %%\n", left * 100.f, right * 100.f);
        }
    }
    else {
        printf("\nTipp: HeadphonesTuner.exe <L%%> <R%%>   z.B.  HeadphonesTuner.exe 100 80\n");
    }

cleanup:
    if (pProps)  pProps->Release();
    if (pVol)    pVol->Release();
    if (pDevice) pDevice->Release();
    if (pEnum)   pEnum->Release();
    CoUninitialize();
    return 0;
}