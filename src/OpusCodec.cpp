/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "xbmc/libXBMC_addon.h"

extern "C" {
#include <opus/opusfile.h>
#include "xbmc/xbmc_audiodec_dll.h"
#include "xbmc/AEChannelData.h"
#include <inttypes.h>

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

//-- Create -------------------------------------------------------------------
// Called on load. Addon should fully initalize or return error status
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

int ReadCallback(void* stream, unsigned char* ptr, int bytes)
{
  return XBMC->ReadFile(stream, ptr, bytes);
}

int SeekCallback(void* stream, opus_int64 offset, int whence)
{
  return XBMC->SeekFile(stream, offset, whence) == offset?0:1;
}

int CloseCallback(void *datasource)
{
  XBMC->CloseFile(datasource);
  return 1;
}

opus_int64 TellCallback(void *datasource)
{
  return XBMC->GetFilePosition(datasource);
}

OpusFileCallbacks GetCallbacks()
{
  OpusFileCallbacks oggIOCallbacks;
  oggIOCallbacks.read  = ReadCallback;
  oggIOCallbacks.seek  = SeekCallback;
  oggIOCallbacks.tell  = TellCallback;
  oggIOCallbacks.close = CloseCallback;

  return oggIOCallbacks;
}

struct OpusContext
{
  void* file;
  OggOpusFile* opusfile;
};

void* Init(const char* strFile, unsigned int filecache, int* channels,
           int* samplerate, int* bitspersample, int64_t* totaltime,
           int* bitrate, AEDataFormat* format, const AEChannel** channelinfo)
{
  OpusContext* result = new OpusContext;

  result->file = XBMC->OpenFile(strFile, 0);
  if (!result->file)
  {
    delete result;
    return NULL;
  }
  OpusFileCallbacks callbacks = GetCallbacks();

  int err;
  result->opusfile = op_open_callbacks(result->file, &callbacks, NULL, 0, &err);
  if (err || !result->opusfile)
  {
    XBMC->Log(ADDON::LOG_ERROR, "OpusCodec: Failed to open %s %i %p", strFile, err, result->opusfile);
    XBMC->CloseFile(result->file);
    delete result;
    return NULL;
  }

  *channels      = op_channel_count(result->opusfile, -1);
  *samplerate    = 48000; 
  *bitspersample = 32;
  *totaltime     = op_pcm_total(result->opusfile, -1)/48000*1000;
  *format        = AE_FMT_FLOAT;
  static enum AEChannel map[8][9] = {
    {AE_CH_FC, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_BL, AE_CH_BR, AE_CH_LFE, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_NULL},
    {AE_CH_FL, AE_CH_FC, AE_CH_FR, AE_CH_SL, AE_CH_SR, AE_CH_BL, AE_CH_BR, AE_CH_LFE, AE_CH_NULL}
  };

  *channelinfo = NULL;
  if (*channels <= 8)
    *channelinfo = map[*channels - 1];
  *bitrate = op_bitrate(result->opusfile, -1);

  if (*samplerate == 0 || *channels == 0 || *bitspersample == 0 || *totaltime == 0)
  {
    XBMC->Log(ADDON::LOG_ERROR, "OGGCodec: Can't get stream info, SampleRate=%i, Channels=%i, BitsPerSample=%i, TotalTime=%"PRIu64"", *samplerate, *channels, *bitspersample, *totaltime);
    delete result;
    return NULL;
  }

  return result;
}

int ReadPCM(void* context, uint8_t* pBuffer, int size, int *actualsize)
{
  OpusContext* ctx = (OpusContext*)context;

  *actualsize=0;
  int iBitStream=-1;

  int li;
  long lRead=op_read_float(ctx->opusfile, (float*)pBuffer, size/sizeof(float), &li);

  if (lRead == OP_HOLE)
    return 0;

  if (lRead<0)
  {
    XBMC->Log(ADDON::LOG_ERROR, "OpusCodec: Read error %lu", lRead);
    return 1;
  }
  else if (lRead==0)
    return -1;
  else
    *actualsize=lRead*sizeof(float)*op_channel_count(ctx->opusfile, -1);

  return 0;
}

int64_t Seek(void* context, int64_t time)
{
  OpusContext* ctx = (OpusContext*)context;

  if (op_pcm_seek(ctx->opusfile, time*48000/1000))
    return 0;

  return time;
}

bool DeInit(void* context)
{
  OpusContext* ctx = (OpusContext*)context;
  op_free(ctx->opusfile);
  XBMC->CloseFile(ctx->file);
  delete ctx;
  return true;
}

bool ReadTag(const char* strFile, char* title, char* artist,
             int* length)
{
  return true;
}

int TrackCount(const char* strFile)
{
  return 1;
}

