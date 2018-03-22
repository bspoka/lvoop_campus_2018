// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

//declarations
extern "C" __declspec(dllexport) void __cdecl picoquant_parse_records(UINT32 *fifo_buffer, UINT64 fifo_size, UINT64 *photon_times, UINT64 *sync, UINT8 *channels,//
	UINT64 *photon_count, UINT64 *overflow, UINT8 device, UINT8 tmode);
extern "C" __declspec(dllexport) void __cdecl picoquant_get_channels(UINT64 *photon_times, UINT8 *sync, UINT8 *channels, UINT8 tmode, UINT64 sync_period, UINT64 num_records, UINT8 ch1,//
	UINT8 ch2, UINT64 *ch1_array, UINT64 *ch2_array, UINT64 *ch1_tally, UINT64 *ch2_tally);


//definitions

//This function is meant to transform the datastream from either picoharp or hydraharp into valid photon records. For T2 data, the output is 64-bit macrotime in ps, 
// and 8-bit ch number. For T3 data, an array of 64-bit sync pulse numbers is also generated. As of 17/08/28, the code doesn't support external markers
__declspec(dllexport) void __cdecl picoquant_parse_records(UINT32 *fifo_buffer, UINT64 fifo_size, UINT64 *photon_times, UINT64 *sync, UINT8 *channels,//
	UINT64 *photon_count, UINT64 *overflow, UINT8 device, UINT8 tmode) {

	UINT32 curr_record;
	UINT32 special;
	UINT32 dtime;
	UINT32 channel;
	UINT64 total_photons = 0;
	UINT64 overflow_correction = *overflow; //starting overflow 

	if (device == 0) { //Hydraharp
		if (tmode == 2) { // Hydraharp T2 mode
			for (int ind = 0; ind < fifo_size; ind++)
			{
				curr_record = fifo_buffer[ind];
				special = (curr_record >> 31) & 1;
				dtime = curr_record & 33554431;
				channel = (curr_record >> 25) & 63;

				UINT64 timetag = overflow_correction + dtime;

				if (special == 0) { //normal photon record
					photon_times[total_photons] = timetag;
					channels[total_photons] = (UINT8)channel;
					total_photons++;
				}
				else {
					if (channel == 63) { //overflow record
						if (dtime == 0)
							overflow_correction += 33554432;
						else
							overflow_correction += 33554432 * dtime;
					}
				}

			}
		}

		if (tmode == 3) {// Hydraharp T3 mode
			UINT32 nsync;
			for (int ind = 0; ind <fifo_size; ind++)
			{
				curr_record = fifo_buffer[ind];
				special = (curr_record >> 31) & 1;
				dtime = (curr_record >> 10) & 32767;
				channel = (curr_record >> 25) & 63;
				nsync = curr_record & 1023;

				if (special == 0) { //normal photon record
					photon_times[total_photons] = dtime;
					channels[total_photons] = (UINT8)channel;
					sync[total_photons] = overflow_correction + nsync;
					total_photons++;
				}
				else {
					if (channel == 63) { //overflow record
						if (nsync == 0)
							overflow_correction += 1024;
						else
							overflow_correction += 1024 * nsync;
					}

				}
			}

		}

		if (device == 1) { //picoharp
			if (tmode == 2) { // picoharp T2 mode
				for (int ind = 0; ind < fifo_size; ind++)
				{
					curr_record = fifo_buffer[ind];
					dtime = curr_record & 268435455;
					channel = (curr_record >> 28) & 15;
					UINT64 timetag = overflow_correction + dtime;

					if ((channel >= 0) && (channel <= 4)) // normal record
					{
						photon_times[total_photons] = timetag;
						channels[total_photons] = (UINT8)channel;
						total_photons++;
					}
					else
					{
						if (channel == 15) {
							UINT32 markers = curr_record & 15;
							if (markers == 0)
								overflow_correction += 210698240;
						}
					}
				}

			}

			if (tmode == 3) { //Picoharp T3 mode
				UINT32 nsync;
				for (int ind = 0; ind < fifo_size; ind++)
				{
					curr_record = fifo_buffer[ind];
					dtime = (curr_record >> 16) & 4095;
					channel = (curr_record >> 28) & 15;
					nsync = curr_record & 65535;

					if ((channel >= 1) && (channel <= 4)) { //normal record
						photon_times[total_photons] = dtime;
						channels[total_photons] = (UINT8)channel;
						sync[total_photons] = overflow_correction + nsync;
					}
					else {
						if (channel == 15) {
							UINT32 markers = (curr_record >> 16) & 15;
							if (markers == 0)
								overflow_correction += 65536; //overflow
						}
					}

				}
			}
		}

		*photon_count = total_photons;
		*overflow = overflow_correction;
	}
}

//this function splits the photonstream into channels
__declspec(dllexport) void __cdecl picoquant_get_channels(UINT64 *photon_times, UINT8 *sync, UINT8 *channels, UINT8 tmode, UINT64 sync_period, UINT64 num_records, UINT8 ch1,//
	UINT8 ch2, UINT64 *ch1_array, UINT64 *ch2_array, UINT64 *ch1_tally, UINT64 *ch2_tally) {

	UINT64 ch1_total = 0;
	UINT64 ch2_total = 0;

	for (int ind = 0; ind < num_records; ind++) {
		if (channels[ind] == ch1) {
			if (tmode == 2)
				ch1_array[ch1_total] = photon_times[ind];
			if (tmode == 3)
				ch1_array[ch1_total] = photon_times[ind] + sync[ind] * sync_period;
			ch1_total++;
		}
		if (channels[ind] == ch2) {
			if (tmode == 2)
				ch2_array[ch2_total] = photon_times[ind];
			if (tmode == 3)
				ch2_array[ch2_total] = photon_times[ind] + sync[ind] * sync_period;
			ch2_total++;
		}
	}

	*ch1_tally = ch1_total;
	*ch2_tally = ch2_total;

}


