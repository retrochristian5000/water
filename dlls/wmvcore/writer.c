/*
 * Copyright 2015 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "wmvcore_private.h"
#include "wmsdkidl.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wmvcore);

struct writer_buffer
{
    INSSBuffer INSSBuffer_iface;
    LONG refcount;
    DWORD capacity;
    DWORD length;
    BYTE data[];
};

static inline struct writer_buffer *impl_from_INSSBuffer(INSSBuffer *iface)
{
    return CONTAINING_RECORD(iface, struct writer_buffer, INSSBuffer_iface);
}

static HRESULT WINAPI writer_buffer_QueryInterface(INSSBuffer *iface, REFIID iid, void **out)
{
    if (!out)
        return E_POINTER;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_INSSBuffer))
    {
        *out = iface;
        INSSBuffer_AddRef(iface);
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI writer_buffer_AddRef(INSSBuffer *iface)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);
    return InterlockedIncrement(&buffer->refcount);
}

static ULONG WINAPI writer_buffer_Release(INSSBuffer *iface)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);
    ULONG refcount = InterlockedDecrement(&buffer->refcount);

    if (!refcount)
        free(buffer);

    return refcount;
}

static HRESULT WINAPI writer_buffer_GetLength(INSSBuffer *iface, DWORD *length)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);

    if (!length)
        return E_POINTER;

    *length = buffer->length;
    return S_OK;
}

static HRESULT WINAPI writer_buffer_SetLength(INSSBuffer *iface, DWORD length)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);

    if (length > buffer->capacity)
        return E_INVALIDARG;

    buffer->length = length;
    return S_OK;
}

static HRESULT WINAPI writer_buffer_GetMaxLength(INSSBuffer *iface, DWORD *length)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);

    if (!length)
        return E_POINTER;

    *length = buffer->capacity;
    return S_OK;
}

static HRESULT WINAPI writer_buffer_GetBuffer(INSSBuffer *iface, BYTE **data)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);

    if (!data)
        return E_POINTER;

    *data = buffer->data;
    return S_OK;
}

static HRESULT WINAPI writer_buffer_GetBufferAndLength(INSSBuffer *iface, BYTE **data, DWORD *length)
{
    struct writer_buffer *buffer = impl_from_INSSBuffer(iface);

    if (!data || !length)
        return E_POINTER;

    *data = buffer->data;
    *length = buffer->length;
    return S_OK;
}

static const INSSBufferVtbl writer_buffer_vtbl =
{
    writer_buffer_QueryInterface,
    writer_buffer_AddRef,
    writer_buffer_Release,
    writer_buffer_GetLength,
    writer_buffer_SetLength,
    writer_buffer_GetMaxLength,
    writer_buffer_GetBuffer,
    writer_buffer_GetBufferAndLength,
};

static HRESULT writer_buffer_create(DWORD capacity, INSSBuffer **out)
{
    struct writer_buffer *buffer;

    if (!out)
        return E_POINTER;

    *out = NULL;
    if ((size_t)capacity > ~(size_t)0 - offsetof(struct writer_buffer, data))
        return E_OUTOFMEMORY;
    if (!(buffer = malloc(offsetof(struct writer_buffer, data) + capacity)))
        return E_OUTOFMEMORY;

    buffer->INSSBuffer_iface.lpVtbl = &writer_buffer_vtbl;
    buffer->refcount = 1;
    buffer->capacity = capacity;
    buffer->length = 0;

    *out = &buffer->INSSBuffer_iface;
    return S_OK;
}

typedef struct {
    IWMWriter IWMWriter_iface;
    IWMWriterAdvanced3 IWMWriterAdvanced3_iface;
    LONG ref;
    IWMWriterSink **sinks;
    DWORD sink_count;
    DWORD sink_capacity;
} WMWriter;

static inline WMWriter *impl_from_IWMWriter(IWMWriter *iface)
{
    return CONTAINING_RECORD(iface, WMWriter, IWMWriter_iface);
}

static HRESULT WINAPI WMWriter_QueryInterface(IWMWriter *iface, REFIID riid, void **ppv)
{
    WMWriter *This = impl_from_IWMWriter(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IWMWriter_iface;
    }else if(IsEqualGUID(&IID_IWMWriter, riid)) {
        TRACE("(%p)->(IID_IWMWriter %p)\n", This, ppv);
        *ppv = &This->IWMWriter_iface;
    }else if(IsEqualGUID(&IID_IWMWriterAdvanced, riid)) {
        TRACE("(%p)->(IID_IWMWriterAdvanced %p)\n", This, ppv);
        *ppv = &This->IWMWriterAdvanced3_iface;
    }else if(IsEqualGUID(&IID_IWMWriterAdvanced2, riid)) {
        TRACE("(%p)->(IID_IWMWriterAdvanced2 %p)\n", This, ppv);
        *ppv = &This->IWMWriterAdvanced3_iface;
    }else if(IsEqualGUID(&IID_IWMWriterAdvanced3, riid)) {
        TRACE("(%p)->(IID_IWMWriterAdvanced3 %p)\n", This, ppv);
        *ppv = &This->IWMWriterAdvanced3_iface;
    }else {
        FIXME("Unsupported iface %s\n", debugstr_guid(riid));
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI WMWriter_AddRef(IWMWriter *iface)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    return ref;
}

static ULONG WINAPI WMWriter_Release(IWMWriter *iface)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%ld\n", This, ref);

    if (!ref)
    {
        DWORD i;

        for (i = 0; i < This->sink_count; ++i)
            IWMWriterSink_Release(This->sinks[i]);
        free(This->sinks);
        free(This);
    }

    return ref;
}

static HRESULT WINAPI WMWriter_SetProfileByID(IWMWriter *iface, REFGUID guidProfile)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_guid(guidProfile));
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_SetProfile(IWMWriter *iface, IWMProfile *profile)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%p)\n", This, profile);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_SetOutputFilename(IWMWriter *iface, const WCHAR *filename)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(filename));
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_GetInputCount(IWMWriter *iface, DWORD *pcInputs)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%p)\n", This, pcInputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_GetInputProps(IWMWriter *iface, DWORD dwInputNum, IWMInputMediaProps **input)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%ld %p)\n", This, dwInputNum, input);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_SetInputProps(IWMWriter *iface, DWORD dwInputNum, IWMInputMediaProps *input)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%ld %p)\n", This, dwInputNum, input);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_GetInputFormatCount(IWMWriter *iface, DWORD dwInputNumber, DWORD *pcFormat)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%ld %p)\n", This, dwInputNumber, pcFormat);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_GetInputFormat(IWMWriter *iface, DWORD dwInputNumber, DWORD dwFormatNumber,
        IWMInputMediaProps **props)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%ld %ld %p)\n", This, dwInputNumber, dwFormatNumber, props);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_BeginWriting(IWMWriter *iface)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_EndWriting(IWMWriter *iface)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_AllocateSample(IWMWriter *iface, DWORD size, INSSBuffer **sample)
{
    WMWriter *This = impl_from_IWMWriter(iface);

    TRACE("(%p)->(%lu %p)\n", This, size, sample);
    return writer_buffer_create(size, sample);
}

static HRESULT WINAPI WMWriter_WriteSample(IWMWriter *iface, DWORD dwInputNum, QWORD cnsSampleTime,
        DWORD flags, INSSBuffer *sample)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)->(%ld %s %lx %p)\n", This, dwInputNum, wine_dbgstr_longlong(cnsSampleTime), flags, sample);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriter_Flush(IWMWriter *iface)
{
    WMWriter *This = impl_from_IWMWriter(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static const IWMWriterVtbl WMWriterVtbl = {
    WMWriter_QueryInterface,
    WMWriter_AddRef,
    WMWriter_Release,
    WMWriter_SetProfileByID,
    WMWriter_SetProfile,
    WMWriter_SetOutputFilename,
    WMWriter_GetInputCount,
    WMWriter_GetInputProps,
    WMWriter_SetInputProps,
    WMWriter_GetInputFormatCount,
    WMWriter_GetInputFormat,
    WMWriter_BeginWriting,
    WMWriter_EndWriting,
    WMWriter_AllocateSample,
    WMWriter_WriteSample,
    WMWriter_Flush
};

static inline WMWriter *impl_from_IWMWriterAdvanced3(IWMWriterAdvanced3 *iface)
{
    return CONTAINING_RECORD(iface, WMWriter, IWMWriterAdvanced3_iface);
}

static HRESULT WINAPI WMWriterAdvanced_QueryInterface(IWMWriterAdvanced3 *iface, REFIID riid, void **ppv)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    return IWMWriter_QueryInterface(&This->IWMWriter_iface, riid, ppv);
}

static ULONG WINAPI WMWriterAdvanced_AddRef(IWMWriterAdvanced3 *iface)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    return IWMWriter_AddRef(&This->IWMWriter_iface);
}

static ULONG WINAPI WMWriterAdvanced_Release(IWMWriterAdvanced3 *iface)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    return IWMWriter_Release(&This->IWMWriter_iface);
}

static HRESULT WINAPI WMWriterAdvanced_GetSinkCount(IWMWriterAdvanced3 *iface, DWORD *sinks)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);

    TRACE("(%p)->(%p)\n", This, sinks);

    if (!sinks)
        return E_POINTER;

    *sinks = This->sink_count;
    return S_OK;
}

static HRESULT WINAPI WMWriterAdvanced_GetSink(IWMWriterAdvanced3 *iface, DWORD sink_num, IWMWriterSink **sink)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);

    TRACE("(%p)->(%lu %p)\n", This, sink_num, sink);

    if (!sink)
        return E_POINTER;

    *sink = NULL;
    if (sink_num >= This->sink_count)
        return E_INVALIDARG;

    IWMWriterSink_AddRef((*sink = This->sinks[sink_num]));
    return S_OK;
}

static HRESULT WINAPI WMWriterAdvanced_AddSink(IWMWriterAdvanced3 *iface, IWMWriterSink *sink)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    IWMWriterSink **sinks;
    DWORD capacity;

    TRACE("(%p)->(%p)\n", This, sink);

    if (!sink)
        return E_INVALIDARG;

    if (This->sink_count == This->sink_capacity)
    {
        capacity = This->sink_capacity ? This->sink_capacity * 2 : 4;
        if (!(sinks = realloc(This->sinks, capacity * sizeof(*sinks))))
            return E_OUTOFMEMORY;

        This->sinks = sinks;
        This->sink_capacity = capacity;
    }

    IWMWriterSink_AddRef(sink);
    This->sinks[This->sink_count++] = sink;
    return S_OK;
}

static HRESULT WINAPI WMWriterAdvanced_RemoveSink(IWMWriterAdvanced3 *iface, IWMWriterSink *sink)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    DWORD i;

    TRACE("(%p)->(%p)\n", This, sink);

    if (!sink)
        return E_INVALIDARG;

    for (i = 0; i < This->sink_count; ++i)
    {
        if (This->sinks[i] != sink)
            continue;

        IWMWriterSink_Release(This->sinks[i]);
        if (i + 1 < This->sink_count)
            memmove(&This->sinks[i], &This->sinks[i + 1],
                    (This->sink_count - i - 1) * sizeof(*This->sinks));
        --This->sink_count;
        return S_OK;
    }

    return E_INVALIDARG;
}

static HRESULT WINAPI WMWriterAdvanced_WriteStreamSample(IWMWriterAdvanced3 *iface, WORD stream_num,
        QWORD sample_time, DWORD sample_send_time, QWORD sample_duration, DWORD flags, INSSBuffer *sample)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%u %s %lu %s %lx %p)\n", This, stream_num, wine_dbgstr_longlong(sample_time),
          sample_send_time, wine_dbgstr_longlong(sample_duration), flags, sample);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_SetLiveSource(IWMWriterAdvanced3 *iface, BOOL is_live_source)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%x)\n", This, is_live_source);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_IsRealTime(IWMWriterAdvanced3 *iface, BOOL *real_time)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%p)\n", This, real_time);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_GetWriterTime(IWMWriterAdvanced3 *iface, QWORD *current_time)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%p)\n", This, current_time);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_GetStatistics(IWMWriterAdvanced3 *iface, WORD stream_num, WM_WRITER_STATISTICS *stats)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%u %p)\n", This, stream_num, stats);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_SetSyncTolerance(IWMWriterAdvanced3 *iface, DWORD window)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%lu)\n", This, window);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced_GetSyncTolerance(IWMWriterAdvanced3 *iface, DWORD *window)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%p)\n", This, window);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced2_GetInputSetting(IWMWriterAdvanced3 *iface, DWORD input_num,
        const WCHAR *name, WMT_ATTR_DATATYPE *time, BYTE *value, WORD *length)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%lu %s %p %p %p)\n", This, input_num, debugstr_w(name), time, value, length);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced2_SetInputSetting(IWMWriterAdvanced3 *iface, DWORD input_num,
        const WCHAR *name, WMT_ATTR_DATATYPE type, const BYTE *value, WORD length)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%lu %s %d %p %u)\n", This, input_num, debugstr_w(name), type, value, length);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced3_GetStatisticsEx(IWMWriterAdvanced3 *iface, WORD stream_num,
        WM_WRITER_STATISTICS_EX *stats)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)->(%u %p)\n", This, stream_num, stats);
    return E_NOTIMPL;
}

static HRESULT WINAPI WMWriterAdvanced3_SetNonBlocking(IWMWriterAdvanced3 *iface)
{
    WMWriter *This = impl_from_IWMWriterAdvanced3(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static const IWMWriterAdvanced3Vtbl WMWriterAdvanced3Vtbl = {
    WMWriterAdvanced_QueryInterface,
    WMWriterAdvanced_AddRef,
    WMWriterAdvanced_Release,
    WMWriterAdvanced_GetSinkCount,
    WMWriterAdvanced_GetSink,
    WMWriterAdvanced_AddSink,
    WMWriterAdvanced_RemoveSink,
    WMWriterAdvanced_WriteStreamSample,
    WMWriterAdvanced_SetLiveSource,
    WMWriterAdvanced_IsRealTime,
    WMWriterAdvanced_GetWriterTime,
    WMWriterAdvanced_GetStatistics,
    WMWriterAdvanced_SetSyncTolerance,
    WMWriterAdvanced_GetSyncTolerance,
    WMWriterAdvanced2_GetInputSetting,
    WMWriterAdvanced2_SetInputSetting,
    WMWriterAdvanced3_GetStatisticsEx,
    WMWriterAdvanced3_SetNonBlocking
};

HRESULT WINAPI WMCreateWriter(IUnknown *reserved, IWMWriter **writer)
{
    WMWriter *ret;

    TRACE("(%p %p)\n", reserved, writer);

    if (!writer || reserved)
        return E_INVALIDARG;

    *writer = NULL;
    if (!(ret = calloc(1, sizeof(*ret))))
        return E_OUTOFMEMORY;

    ret->IWMWriter_iface.lpVtbl = &WMWriterVtbl;
    ret->IWMWriterAdvanced3_iface.lpVtbl = &WMWriterAdvanced3Vtbl;
    ret->ref = 1;

    *writer = &ret->IWMWriter_iface;
    return S_OK;
}

HRESULT WINAPI WMCreateWriterPriv(IWMWriter **writer)
{
    return WMCreateWriter(NULL, writer);
}
