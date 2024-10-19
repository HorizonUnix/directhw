#ifndef __DIRECTHW_HPP__
#define __DIRECTHW_HPP__

/* DirectHW - Kernel extension to pass through IO commands to user space
 *
 * Copyright © 2008-2010 coresystems GmbH <info@coresystems.de>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "MacOSMacros.h"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOMemoryDescriptor.h>

#ifndef DIRECTHW_VERSION
    #define DIRECTHW_VERSION "1.6.1"
#endif

#ifndef DIRECTHW_VERNUM
    #define DIRECTHW_VERNUM 0x00100500
#endif

#ifndef APPLE_KEXT_OVERRIDE
    #ifdef __clang__
        #define APPLE_KEXT_OVERRIDE override
    #else
        #define APPLE_KEXT_OVERRIDE
    #endif
#endif

#ifndef LIBKERN_RETURNS_NOT_RETAINED
    #define LIBKERN_RETURNS_NOT_RETAINED
#endif

#ifndef rdmsr
    #define rdmsr(msr, lo, hi) \
    __asm__ volatile("rdmsr" : "=a" (lo), "=d" (hi) : "c" (msr))
#endif

#ifndef wrmsr
    #define wrmsr(msr, lo, hi) \
    __asm__ volatile("wrmsr" : : "c" (msr), "a" (lo), "d" (hi))
#endif

class DirectHWService : public IOService
{
    OSDeclareDefaultStructors(DirectHWService)

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

class DirectHWUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(DirectHWUserClient)

    enum {
        kReadIO,
        kWriteIO,
        kPrepareMap,
        kReadMSR,
        kWriteMSR,
        kReadCpuId,
        kReadMem,
        kRead,
        kWrite,
        kNumberOfMethods
    };

    typedef struct {
        UInt64 offset;
        UInt64 width;
        UInt64 data; // this field is always little endian
    } iomem64_t;

    typedef struct {
        UInt32 offset;
        UInt32 width;
        UInt32 data; // this field is always little endian
    } iomem_t;

    typedef struct {
        UInt64 addr;
        UInt64 size;
    } map_t;

    typedef struct {
        UInt32 addr;
        UInt32 size;
    } map32_t;

    typedef struct {
        UInt32 core;
        UInt32 index;
        UInt32 hi;
        UInt32 lo;
    } msrcmd_t;

    typedef struct {
        uint32_t core;
        uint32_t eax;
        uint32_t ecx;
        uint32_t cpudata[4];
    } cpuid_t;

     typedef struct {
        uint32_t core;
        uint64_t addr;
        uint32_t data;
    } readmem_t;

    /* Space definitions */
    enum {
        kConfigSpace           = 0,
        kIOSpace               = 1,
        k32BitMemorySpace      = 2,
        k64BitMemorySpace      = 3
    };

    union Address {
        uint64_t addr64;
        struct {
            unsigned int offset     :16;
            unsigned int function   :3;
            unsigned int device     :5;
            unsigned int bus        :8;
            unsigned int segment    :16;
            unsigned int reserved   :16;
        } pci;
        struct {
            unsigned int reserved   :16;
            unsigned int segment    :16;
            unsigned int bus        :8;
            unsigned int device     :5;
            unsigned int function   :3;
            unsigned int offset     :16;
        } pciswapped;
    };
    typedef union Address Address;

    struct Parameters {
        uint32_t options;
        uint32_t spaceType;
        uint32_t bitWidth;
        uint32_t _resv;
        uint64_t value;
        Address  address;
    };
    typedef struct Parameters Parameters;


public:
    virtual bool initWithTask(task_t task, void *securityID, UInt32 type, OSDictionary* properties) APPLE_KEXT_OVERRIDE;

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService * provider) APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory) APPLE_KEXT_OVERRIDE;

    virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;

protected:
    DirectHWService *fProvider;

    static const IOExternalMethod fMethods[kNumberOfMethods];
    static const IOExternalAsyncMethod fAsyncMethods[kNumberOfMethods];

    virtual IOExternalMethod *getTargetAndMethodForIndex(LIBKERN_RETURNS_NOT_RETAINED IOService ** target, UInt32 index) APPLE_KEXT_OVERRIDE;
    virtual IOExternalAsyncMethod *getAsyncTargetAndMethodForIndex(LIBKERN_RETURNS_NOT_RETAINED IOService ** target, UInt32 index) APPLE_KEXT_OVERRIDE;

    virtual IOReturn ReadIO(iomem_t *inStruct, iomem_t *outStruct,
                            IOByteCount inStructSize,
                            IOByteCount *outStructSize);

    virtual IOReturn ReadIOAsync(OSAsyncReference asyncRef,
                                 iomem_t *inStruct, iomem_t *outStruct,
                                 IOByteCount inStructSize,
                                 IOByteCount *outStructSize);

    virtual IOReturn WriteIO(iomem_t *inStruct, iomem_t *outStruct,
                             IOByteCount inStructSize,
                             IOByteCount *outStructSize);

    virtual IOReturn WriteIOAsync(OSAsyncReference asyncRef,
                                  iomem_t *inStruct, iomem_t *outStruct,
                                  IOByteCount inStructSize,
                                  IOByteCount *outStructSize);

    virtual IOReturn PrepareMap(map_t *inStruct, map_t *outStruct,
                                IOByteCount inStructSize,
                                IOByteCount *outStructSize);

    virtual IOReturn PrepareMapAsync(OSAsyncReference asyncRef,
                                     map_t *inStruct, map_t *outStruct,
                                     IOByteCount inStructSize,
                                     IOByteCount *outStructSize);

    virtual IOReturn ReadMSR(msrcmd_t *inStruct, msrcmd_t *outStruct,
                             IOByteCount inStructSize,
                             IOByteCount *outStructSize);

    virtual IOReturn ReadMSRAsync(OSAsyncReference asyncRef,
                                  msrcmd_t *inStruct, msrcmd_t *outStruct,
                                  IOByteCount inStructSize,
                                  IOByteCount *outStructSize);

    virtual IOReturn WriteMSR(msrcmd_t *inStruct, msrcmd_t *outStruct,
                              IOByteCount inStructSize,
                              IOByteCount *outStructSize);

    virtual IOReturn WriteMSRAsync(OSAsyncReference asyncRef,
                                   msrcmd_t *inStruct, msrcmd_t *outStruct,
                                   IOByteCount inStructSize,
                                   IOByteCount *outStructSize);

    virtual IOReturn ReadCpuId(cpuid_t * inStruct, cpuid_t * outStruct,
                               IOByteCount inStructSize,
                               IOByteCount * outStructSize);

    virtual IOReturn ReadCpuIdAsync(OSAsyncReference asyncRef,
                                    cpuid_t * inStruct, cpuid_t * outStruct,
                                    IOByteCount inStructSize,
                                    IOByteCount * outStructSize);

    virtual IOReturn ReadMem(readmem_t * inStruct, readmem_t * outStruct,
                             IOByteCount inStructSize,
                             IOByteCount * outStructSize);

    virtual IOReturn ReadMemAsync(OSAsyncReference asyncRef,
                                  readmem_t * inStruct, readmem_t * outStruct,
                                  IOByteCount inStructSize,
                                  IOByteCount * outStructSize);

    virtual IOReturn Read(Parameters * inStruct, Parameters * outStruct,
                          IOByteCount inStructSize,
                          IOByteCount * outStructSize);

    virtual IOReturn ReadAsync(OSAsyncReference asyncRef,
                               Parameters * inStruct, Parameters * outStruct,
                               IOByteCount inStructSize,
                               IOByteCount * outStructSize);

    virtual IOReturn Write(Parameters * inStruct, Parameters * outStruct,
                           IOByteCount inStructSize,
                           IOByteCount * outStructSize);

    virtual IOReturn WriteAsync(OSAsyncReference asyncRef,
                                Parameters * inStruct, Parameters * outStruct,
                                IOByteCount inStructSize,
                                IOByteCount * outStructSize);

    virtual IOReturn ReadWrite(uint32_t selector,
                               Parameters * inStruct, Parameters * outStruct,
                               IOByteCount inStructSize,
                               IOByteCount * outStructSize);

private:
    task_t fTask;
    bool fCrossEndian;

    UInt64 LastMapAddr;
    UInt64 LastMapSize;

    static void MSRHelperFunction(void *data);
    static void CPUIDHelperFunction(void *data);
    static void ReadMemHelperFunction(void *data);

    typedef struct {
        msrcmd_t *in;
        msrcmd_t *out;
        bool Read;
    } MSRHelper;

    typedef struct {
        cpuid_t *in, *out;
    } CPUIDHelper;

    typedef struct {
        readmem_t *in, *out;
    } ReadMemHelper;

    static inline void cpuid(uint32_t op1, uint32_t op2, uint32_t *data);

    void GetPciHostBridges1(IOService *service, OSIterator *services);
    void GetPciHostBridges(void);
	IOPCIDevice * FindMatching(IOService *service, IOPCIAddressSpace space, OSIterator *services);
};

#ifndef INVALID_MSR_LO
    #define INVALID_MSR_LO 0x63744857
#endif

#ifndef INVALID_MSR_HI
    #define INVALID_MSR_HI 0x44697265
#endif

#endif /* __DIRECTHW_HPP__ */
