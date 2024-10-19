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

#include "DirectHW.hpp"
#include <IOKit/pci/IOPCIBridge.h>

#if defined(__i386__) || defined(__x86_64__)
    #if 0
        #include <architecture/i386/pio.h>
    #else
        typedef unsigned short i386_ioport_t;
        #if defined(__GNUC__)
            static __inline__ UInt32 inl (i386_ioport_t port) { UInt32 datum;   __asm__ volatile("inl  %w1,  %0" :  "=a" (datum) : "Nd" (port)); return(datum); }
            static __inline__ UInt16 inw (i386_ioport_t port) { UInt16 datum;   __asm__ volatile("inw  %w1, %w0" :  "=a" (datum) : "Nd" (port)); return(datum); }
            static __inline__ UInt8  inb (i386_ioport_t port) { UInt8  datum;   __asm__ volatile("inb  %w1, %b0" :  "=a" (datum) : "Nd" (port)); return(datum); }
            static __inline__ void   outl(i386_ioport_t port,   UInt32 datum) { __asm__ volatile("outl  %0, %w1" : : "a" (datum) , "Nd" (port)); }
            static __inline__ void   outw(i386_ioport_t port,   UInt16 datum) { __asm__ volatile("outw %w0, %w1" : : "a" (datum) , "Nd" (port)); }
            static __inline__ void   outb(i386_ioport_t port,   UInt8  datum) { __asm__ volatile("outb %b0, %w1" : : "a" (datum) , "Nd" (port)); }
        #endif
    #endif
#endif

//#define DOLOG kprintf
#define DOLOG IOLog

//This is defined in the compiler flags for the debug target.
//#undef DEBUG_KEXT
//#define DEBUG_KEXT

#undef  super
#define super IOService

#if MAC_OS_X_VERSION_SDK <= MAC_OS_X_VERSION_10_5
    #define kIOMemoryMapperNone kIOMemoryDontMap
#endif

#if MAC_OS_X_VERSION_SDK <= MAC_OS_X_VERSION_10_4
    #define kIOUCVariableStructureSize ((IOByteCount)-1)
    #define getAddress getVirtualAddress
#endif

extern "C"
{
    /* from sys/osfmk/i386/mp.c */
#if MAC_OS_X_VERSION_SDK <= MAC_OS_X_VERSION_10_5 || defined(__arm64e__)
    #if defined(__i386__) || defined(__x86_64__)
        extern void mp_rendezvous(void (*setup_func)(void *),
                                  void (*action_func)(void *),
                                  void (*teardown_func)(void *),
                                  void *arg);
    #else
        static void mp_rendezvous(void (*setup_func)(void *),
                                  void (*action_func)(void *),
                                  void (*teardown_func)(void *),
                                  void *arg)
        {
            ((void)setup_func);
            ((void)teardown_func);
            action_func(arg);
        }
    #endif

    #define mp_rendezvous_no_intrs(x, y) mp_rendezvous(NULL, x, NULL, y)

    #define cpu_number() (0)

#else
        extern void mp_rendezvous(void (*setup_func)(void *),
                                  void (*action_func)(void *),
                                  void (*teardown_func)(void *),
                                  void *arg);

        extern void mp_rendezvous_no_intrs(void (*action_func)(void *),
                                           void *arg) /* __attribute__((weak_import)) */;

        extern int cpu_number(void) /* __attribute__((weak_import)) */ ;
#endif
}

OSDefineMetaClassAndStructors(DirectHWService, IOService)

bool DirectHWService::start(IOService * provider)
{
    DOLOG("DirectHW: Driver v%s (compiled on %s) loaded.\n", DIRECTHW_VERSION, __DATE__);
    DOLOG("Visit http://www.coresystems.de/ for more information.\n");

    if (super::start(provider)) {
        registerService();
        return true;
    }

    return false;
}

#undef  super
#define super IOUserClient

OSDefineMetaClassAndStructors(DirectHWUserClient, IOUserClient)

const IOExternalAsyncMethod DirectHWUserClient::fAsyncMethods[kNumberOfMethods] = {
    {0, (IOAsyncMethod) & DirectHWUserClient::ReadIOAsync, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOAsyncMethod) & DirectHWUserClient::WriteIOAsync, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOAsyncMethod) & DirectHWUserClient::PrepareMapAsync, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOAsyncMethod) & DirectHWUserClient::ReadMSRAsync, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
    {0, (IOAsyncMethod) & DirectHWUserClient::WriteMSRAsync, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
    {0, (IOAsyncMethod) & DirectHWUserClient::ReadCpuIdAsync, kIOUCStructIStructO, sizeof(cpuid_t), sizeof(cpuid_t)},
    {0, (IOAsyncMethod) & DirectHWUserClient::ReadMemAsync, kIOUCStructIStructO, sizeof(readmem_t), sizeof(readmem_t)},
    {0, (IOAsyncMethod) & DirectHWUserClient::ReadAsync, kIOUCStructIStructO, sizeof(Parameters), sizeof(Parameters)},
    {0, (IOAsyncMethod) & DirectHWUserClient::WriteAsync, kIOUCStructIStructO, sizeof(Parameters), sizeof(Parameters)},
};

const IOExternalMethod DirectHWUserClient::fMethods[kNumberOfMethods] = {
    {0, (IOMethod) & DirectHWUserClient::ReadIO, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOMethod) & DirectHWUserClient::WriteIO, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOMethod) & DirectHWUserClient::PrepareMap, kIOUCStructIStructO, kIOUCVariableStructureSize, kIOUCVariableStructureSize},
    {0, (IOMethod) & DirectHWUserClient::ReadMSR, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
    {0, (IOMethod) & DirectHWUserClient::WriteMSR, kIOUCStructIStructO, sizeof(msrcmd_t), sizeof(msrcmd_t)},
    {0, (IOMethod) & DirectHWUserClient::ReadCpuId, kIOUCStructIStructO, sizeof(cpuid_t), sizeof(cpuid_t)},
    {0, (IOMethod) & DirectHWUserClient::ReadMem, kIOUCStructIStructO, sizeof(readmem_t), sizeof(readmem_t)},
    {0, (IOMethod) & DirectHWUserClient::Read, kIOUCStructIStructO, sizeof(Parameters), sizeof(Parameters)},
    {0, (IOMethod) & DirectHWUserClient::Write, kIOUCStructIStructO, sizeof(Parameters), sizeof(Parameters)},
};

bool DirectHWUserClient::initWithTask(task_t task, void *securityID, UInt32 type, OSDictionary* properties)
{
    bool ret;

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: initWithTask(%p, %p, %lx)\n", (void *)task, (void *)securityID, (unsigned long)type);
    #endif

    if (kIOReturnSuccess != clientHasPrivilege(securityID, kIOClientPrivilegeAdministrator)) {
        DOLOG("DirectHW: Requires administrator.\n");
        return (false);
    }

    ret = super::initWithTask(task, securityID, type);
    if (ret == false) {
        DOLOG("DirectHW: initWithTask failed.\n");
        return ret;
    }

    fCrossEndian = false;

#if MAC_OS_X_VERSION_SDK >= MAC_OS_X_VERSION_10_4
    if (properties != NULL && properties->getObject(kIOUserClientCrossEndianKey)) {
        // A connection to this user client is being opened by a user process running using Rosetta.

        // Indicate that this user client can handle being called from cross-endian user processes by
        // setting its IOUserClientCrossEndianCompatible property in the I/O Registry.
        if (setProperty(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue)) {
            fCrossEndian = true;
            DOLOG("DirectHW: fCrossEndian = true\n");
        }
    }
#endif

    fTask = task;
    return ret;
}

IOExternalAsyncMethod *DirectHWUserClient::getAsyncTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    if (target == NULL) {
        DOLOG("DirectHW: getAsyncTargetAndMethodForIndex no target\n");
        return NULL;
    }

    if (index < (UInt32) kNumberOfMethods) {
        if (fAsyncMethods[index].object == (IOService *) 0) {
            *target = this;
        }
        return (IOExternalAsyncMethod *) & fAsyncMethods[index];
    }

    DOLOG("DirectHW: getAsyncTargetAndMethodForIndex index %d out of range %d\n", (int)index, (int)kNumberOfMethods);
    *target = NULL;
    return NULL;
}

IOExternalMethod *DirectHWUserClient::getTargetAndMethodForIndex(IOService ** target, UInt32 index)
{
    if (target == NULL) {
        return NULL;
    }

    if (index < (UInt32) kNumberOfMethods) {
        if (fMethods[index].object == (IOService *) 0) {
            *target = this;
        }

        return (IOExternalMethod *) & fMethods[index];
    }

    DOLOG("DirectHW: getTargetAndMethodForIndex index %d out of range %d\n", (int)index, (int)kNumberOfMethods);
    *target = NULL;
    return NULL;
}

bool DirectHWUserClient::start(IOService * provider)
{
    bool success;

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: Starting DirectHWUserClient.\n");
    #endif

    fProvider = OSDynamicCast(DirectHWService, provider);
    success = (fProvider != NULL);

    if (success) {
        success = super::start(provider);
        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Client successfully started.\n");
        #endif
    }
    else {
        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Could not start client.\n");
        #endif
    }

#if 0
    #if (defined(__i386__) || defined(__x86_64__))
        uint32_t cr0, cr2, cr3;
        #ifdef __x86_64__
            __asm__ __volatile__ (
                "mov %%cr0, %%rax\n"
                "mov %%eax, %0\n"
                "mov %%cr2, %%rax\n"
                "mov %%eax, %1\n"
                "mov %%cr3, %%rax\n"
                "mov %%eax, %2\n"
                : "=m" (cr0), "=m" (cr2), "=m" (cr3)
                : /* no input */
                : "%rax"
            );
        #elif defined(__i386__)
            __asm__ __volatile__ (
                "mov %%cr0, %%eax\n"
                "mov %%eax, %0\n"
                "mov %%cr2, %%eax\n"
                "mov %%eax, %1\n"
                "mov %%cr3, %%eax\n"
                "mov %%eax, %2\n"
                : "=m" (cr0), "=m" (cr2), "=m" (cr3)
                : /* no input */
                : "%eax"
            );
        #endif
        DOLOG("DirectHW: cr0 = 0x%8.8X\n", cr0);
        DOLOG("DirectHW: cr2 = 0x%8.8X\n", cr2);
        DOLOG("DirectHW: cr3 = 0x%8.8X\n", cr3);
    #endif
#endif
    return success;
}

void DirectHWUserClient::stop(IOService *provider)
{
    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: Stopping client.\n");
    #endif

    super::stop(provider);
}

IOReturn DirectHWUserClient::clientClose(void)
{
    bool success = terminate();
    if (!success) {
        DOLOG("DirectHW: Client NOT successfully closed.\n");
    }
    else {
        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Client successfully closed.\n");
        #endif
    }

    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadIOAsync(OSAsyncReference asyncRef,
                                iomem_t *inStruct, iomem_t *outStruct,
                                IOByteCount inStructSize,
                                IOByteCount *outStructSize)
{
    ((void)asyncRef);
    return ReadIO(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::ReadIO(iomem_t *inStruct, iomem_t *outStruct,
                           IOByteCount inStructSize,
                           IOByteCount *outStructSize)
{
#if defined(__i386__) || defined(__x86_64__)
    if (
        (inStructSize != sizeof(iomem_t) && inStructSize != sizeof(iomem64_t))
        || !outStructSize
        || *outStructSize != inStructSize
    ) {
        return kIOReturnBadArgument;
    }

    if ((fProvider == NULL) || (isInactive())) {
        return kIOReturnNotAttached;
    }

    if (inStructSize == sizeof(iomem_t)) {
        if (fCrossEndian) {
            inStruct->offset = OSSwapInt32(inStruct->offset);
            inStruct->width = OSSwapInt32(inStruct->width);
        }

        outStruct->data = 0;
        switch (inStruct->width) {
            case 1: *(UInt8*)(&outStruct->data) = inb(inStruct->offset); break;
            case 2: *(UInt16*)(&outStruct->data) = inw(inStruct->offset); break;
            case 4: {
                        UInt64 val = inl(inStruct->offset);
                        *(UInt32*)(&outStruct->data) = (UInt32)val;
                    } break;
            default:
                DOLOG("DirectHW: Invalid read attempt %ld bytes at IO address %lx\n",
                      (long)inStruct->width, (unsigned long)inStruct->offset);
                return kIOReturnBadArgument;
        }

        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Read %ld bytes at IO address %lx (result=%lx)\n",
                  (unsigned long)inStruct->width, (unsigned long)inStruct->offset, (unsigned long)outStruct->data);
        #endif

        if (fCrossEndian) {
            switch (inStruct->width) {
                case 2: *(UInt16*)(&outStruct->data) = OSSwapInt16(*(UInt16*)(&outStruct->data)); break;
                case 4: *(UInt32*)(&outStruct->data) = OSSwapInt32(*(UInt32*)(&outStruct->data)); break;
            }
        }
    }
    else {
        iomem64_t *inStruct64 = (iomem64_t*)inStruct;
        iomem64_t *outStruct64 = (iomem64_t*)outStruct;

        if (fCrossEndian) {
            inStruct64->offset = OSSwapInt64(inStruct64->offset);
            inStruct64->width = OSSwapInt64(inStruct64->width);
        }

        switch (inStruct64->width) {
            case 1: *(UInt8*)(&outStruct64->data) = inb(inStruct64->offset); break;
            case 2: *(UInt16*)(&outStruct64->data) = inw(inStruct64->offset); break;
            case 4: {
                        UInt64 val = inl((i386_ioport_t)inStruct64->offset);
                        *(UInt32*)(&outStruct64->data) = (UInt32)val;
                    } break;
            case 8: {
                        UInt64 val = inl((i386_ioport_t)inStruct64->offset);
                        UInt64 val2 = inl((i386_ioport_t)inStruct64->offset + 4);
                        *(UInt64*)(&outStruct64->data) = (UInt64)(val) | ((UInt64)(val2) << 32);
                    } break;
            default:
                DOLOG("DirectHW: Invalid read attempt %ld bytes at IO address %lx\n",
                      (long)inStruct64->width, (unsigned long)inStruct64->offset);
                return kIOReturnBadArgument;
        }

        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Read %ld bytes at IO address %lx (result=%lx)\n",
                  (unsigned long)inStruct64->width, (unsigned long)inStruct64->offset, (unsigned long)outStruct64->data);
        #endif

        if (fCrossEndian) {
            switch (inStruct64->width) {
                case 2: *(UInt16*)(&outStruct64->data) = OSSwapInt16(*(UInt16*)(&outStruct64->data)); break;
                case 4: *(UInt32*)(&outStruct64->data) = OSSwapInt32(*(UInt32*)(&outStruct64->data)); break;
                case 8: *(UInt64*)(&outStruct64->data) = OSSwapInt64(*(UInt64*)(&outStruct64->data)); break;
            }
        }
    }
#else
    ((void)inStruct);
    ((void)outStruct);
    ((void)inStructSize);
    ((void)outStructSize);
    return kIOReturnBadArgument;
#endif

    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::WriteIOAsync(OSAsyncReference asyncRef, iomem_t *inStruct, iomem_t *outStruct,
                                 IOByteCount inStructSize,
                                 IOByteCount *outStructSize)
{
    ((void)asyncRef);
    return WriteIO(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::WriteIO(iomem_t *inStruct, iomem_t *outStruct,
                            IOByteCount inStructSize,
                            IOByteCount *outStructSize)
{
    ((void)outStruct);

#if defined(__i386__) || defined(__x86_64__)
    if (
        (inStructSize != sizeof(iomem_t) && inStructSize != sizeof(iomem64_t))
        || !outStructSize
        || *outStructSize != inStructSize
    ) {
        return kIOReturnBadArgument;
    }

    if ((fProvider == NULL) || (isInactive())) {
        return kIOReturnNotAttached;
    }

    if (inStructSize == sizeof(iomem_t)) {
        if (fCrossEndian) {
            inStruct->offset = OSSwapInt32(inStruct->offset);
            inStruct->width = OSSwapInt32(inStruct->width);
            switch (inStruct->width) {
                case 2: *(UInt16*)(&inStruct->data) = OSSwapInt16(*(UInt16*)(&inStruct->data)); break;
                case 4: *(UInt32*)(&inStruct->data) = OSSwapInt32(*(UInt32*)(&inStruct->data)); break;
            }
        }

        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Write %ld bytes at IO address %lx (value=%lx)\n",
                  (long)inStruct->width, (unsigned long)inStruct->offset, (unsigned long)inStruct->data);
        #endif

        switch (inStruct->width) {
            case 1: outb(inStruct->offset, *(UInt8*)(&inStruct->data)); break;
            case 2: outw(inStruct->offset, *(UInt16*)(&inStruct->data)); break;
            case 4: {
                        unsigned int val = (unsigned int)inStruct->data;
                        outl(inStruct->offset, val);
                    } break;
            default:
                DOLOG("DirectHW: Invalid write attempt %ld bytes at IO address %lx\n",
                      (long)inStruct->width, (unsigned long)inStruct->offset);
                return kIOReturnBadArgument;
        }
    }
    else {
        iomem64_t *inStruct64 = (iomem64_t*)inStruct;

        if (fCrossEndian) {
            inStruct64->offset = OSSwapInt64(inStruct64->offset);
            inStruct64->width = OSSwapInt64(inStruct64->width);
            switch (inStruct64->width) {
                case 2: *(UInt16*)(&inStruct64->data) = OSSwapInt16(*(UInt16*)(&inStruct64->data)); break;
                case 4: *(UInt32*)(&inStruct64->data) = OSSwapInt32(*(UInt32*)(&inStruct64->data)); break;
                case 8: *(UInt64*)(&inStruct64->data) = OSSwapInt64(*(UInt64*)(&inStruct64->data)); break;
            }
        }

        #ifdef DEBUG_KEXT
            DOLOG("DirectHW: Write %ld bytes at IO address %lx (value=%lx)\n",
                  (long)inStruct64->width, (unsigned long)inStruct64->offset, (unsigned long)inStruct64->data);
        #endif

        switch (inStruct64->width) {
            case 1: outb(inStruct64->offset, (unsigned char)inStruct64->data); break;
            case 2: outw(inStruct64->offset, (unsigned short)inStruct64->data); break;
            case 4: {
                        unsigned int val = (unsigned int)inStruct64->data;
                        outl(inStruct64->offset, val);
                    } break;
            case 8: {
                        unsigned int val = (unsigned int)((UInt32)inStruct64->data);
                        unsigned int val2 = (unsigned int)(inStruct64->data >> 32);
                        outl(inStruct64->offset, val);
                        outl(inStruct64->offset + 4, val2);
                    } break;
            default:
                DOLOG("DirectHW: Invalid write attempt %ld bytes at IO address %lx\n",
                      (long)inStruct64->width, (unsigned long)inStruct64->offset);
                return kIOReturnBadArgument;
        }
    }
#else
    ((void)inStruct);
    ((void)inStructSize);
    ((void)outStructSize);
    return kIOReturnBadArgument;
#endif

    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::PrepareMapAsync(OSAsyncReference asyncRef,
                                    map_t *inStruct, map_t *outStruct,
                                    IOByteCount inStructSize,
                                    IOByteCount *outStructSize)
{
    ((void)asyncRef);
    return PrepareMap(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::PrepareMap(map_t *inStruct, map_t *outStruct,
                               IOByteCount inStructSize,
                               IOByteCount *outStructSize)
{
    if (
        (inStructSize != sizeof(map_t) && inStructSize != sizeof(map32_t))
        || !outStructSize
        || *outStructSize != inStructSize
    ) {
        return kIOReturnBadArgument;
    }

    ((void)outStruct);
    ((void)inStructSize);

    if ((fProvider == NULL) || (isInactive())) {
        return kIOReturnNotAttached;
    }

    if ((LastMapAddr != 0) || (LastMapSize != 0)) {
        return kIOReturnNotOpen;
    }

    if (inStructSize == sizeof(map_t)) {
        if (fCrossEndian) {
            inStruct->addr = OSSwapInt64(inStruct->addr);
            inStruct->size = OSSwapInt64(inStruct->size);
        }
        LastMapAddr = inStruct->addr;
        LastMapSize = inStruct->size;
    }
    else {
        map32_t *inStruct32 = (map32_t *)inStruct;
        if (fCrossEndian) {
            inStruct32->addr = OSSwapInt32(inStruct32->addr);
            inStruct32->size = OSSwapInt32(inStruct32->size);
        }
        LastMapAddr = inStruct32->addr;
        LastMapSize = inStruct32->size;
    }

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: PrepareMap 0x%16lx[0x%lx]\n", (unsigned long)LastMapAddr, (unsigned long)LastMapSize);
    #endif

    return kIOReturnSuccess;
}

inline void
DirectHWUserClient::cpuid(uint32_t op1, uint32_t op2, uint32_t *data)
{
#if defined(__i386__) || defined(__x86_64__)
    asm("cpuid"
        : "=a" (data[0]),
        "=b" (data[1]),
        "=c" (data[2]),
        "=d" (data[3])
        : "a"(op1),
        "c"(op2));
#else
    ((void)op1);
    ((void)op2);
    data[0] = data[1] = data[2] = data[3] = 0;
#endif
}

static inline uint64_t
rdmsr64(uint32_t msr)
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint64_t val;

#if defined(__i386__) || defined(__x86_64__)
    rdmsr(msr, lo, hi);
#else
    ((void)msr);
#endif

    val = (((uint64_t)hi) << 32) | ((uint64_t)lo);

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: rdmsr64(0x%.16lX) => %.16llX\n", (unsigned long)msr, (unsigned long long)val);
    #endif

    return val;
}

static inline void wrmsr64(UInt32 msr, UInt64 val)
{
    UInt32 lo = (UInt32)val;
    UInt32 hi = (UInt32)(val >> 32);

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: wrmsr64(0x%.16lX, %.16llX)\n", (unsigned long)msr, (unsigned long long)val);
    #endif

#if defined(__i386__) || defined(__x86_64__)
    wrmsr(msr, lo, hi);
#else
    ((void)msr);
    ((void)lo);
    ((void)hi);
#endif
}

void
DirectHWUserClient::CPUIDHelperFunction(void *data)
{
    CPUIDHelper * cpuData = (CPUIDHelper *)data;
    cpuData->out->core = (UInt32)-1;
    if (cpuData->in->core != cpu_number())
        return;
    cpuid(cpuData->in->eax, cpuData->in->ecx, cpuData->out->cpudata);
    cpuData->out->eax = cpuData->in->eax;
    cpuData->out->ecx = cpuData->in->ecx;
    cpuData->out->core = cpuData->in->core;
}

void
DirectHWUserClient::ReadMemHelperFunction(void *data)
{
    ReadMemHelper * memData = (ReadMemHelper *)data;
    memData->out->core = (UInt32)-1;
    if (memData->in->core != cpu_number())
        return;
    uint32_t out;
#if defined(__i386__) || defined(__x86_64__)
    uint64_t addr = memData->in->addr;
    __asm__ __volatile__ (
        "mov %1,%%eax\n"
        "mov %%eax, %0\n"
        : "=m" (out)
        : "m" (addr)
        : "%eax"
    );
#else
    out = 0;
#endif
    memData->out->data = out;
    memData->out->core = memData->in->core;
}

void
DirectHWUserClient::MSRHelperFunction(void *data)
{
    MSRHelper *MSRData = (MSRHelper *)data;
    msrcmd_t *inStruct = MSRData->in;
    msrcmd_t *outStruct = MSRData->out;

    outStruct->core = ((UInt32)-1);

    outStruct->lo = INVALID_MSR_LO;
    outStruct->hi = INVALID_MSR_HI;

    uint32_t cpuiddata[4];

    cpuid(1, 0, cpuiddata);

    //bool have_ht = ((cpuiddata[3] & (1 << 28)) != 0);

    uint32_t core_id = cpuiddata[1] >> 24;

    cpuid(11, 0, cpuiddata);

    uint32_t smt_mask = ~((-1) << (cpuiddata[0] &0x1f));

    // TODO: What we want is this:
    // if (inStruct->core != cpu_to_core(cpu_number()))
    //     return;

    if ((core_id & smt_mask) != core_id) {
        return; // It's a HT thread
    }

    if (inStruct->core != cpu_number()) {
        return;
    }

    DOLOG("DirectHW: ReadMSRHelper %ld %ld %lx\n",
          (long)inStruct->core, (long)cpu_number(), (unsigned long)smt_mask);

    if (MSRData->Read) {
        uint64_t ret = rdmsr64(inStruct->index);
        outStruct->lo = (uint32_t)ret;
        outStruct->hi = (uint32_t)(ret >> 32);
    }
    else {
        wrmsr64(inStruct->index, ((uint64_t)inStruct->hi << 32) | inStruct->lo);
    }

    outStruct->index = inStruct->index;
    outStruct->core = inStruct->core;
}

IOReturn
DirectHWUserClient::ReadMSRAsync(OSAsyncReference asyncRef,
                                 msrcmd_t *inStruct, msrcmd_t *outStruct,
                                 IOByteCount inStructSize,
                                 IOByteCount *outStructSize)
{
    ((void)asyncRef);
    return ReadMSR(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::ReadMSR(msrcmd_t *inStruct, msrcmd_t *outStruct,
                            IOByteCount inStructSize,
                            IOByteCount *outStructSize)
{
    ((void)inStructSize);

    if ((fProvider == NULL) || (isInactive())) {
        return kIOReturnNotAttached;
    }

    if (fCrossEndian) {
        inStruct->core = OSSwapInt32(inStruct->core);
        inStruct->index = OSSwapInt32(inStruct->index);
        inStruct->lo = OSSwapInt32(inStruct->lo);
        inStruct->hi = OSSwapInt32(inStruct->hi);
    }

    MSRHelper MSRData = { inStruct, outStruct, true };

    #ifdef USE_MP_RENDEZVOUS
        mp_rendezvous(NULL, (void (*)(void *))MSRHelperFunction, NULL, (void *)&MSRData);
    #else
        mp_rendezvous_no_intrs((void (*)(void *))MSRHelperFunction, (void *)&MSRData);
    #endif

    if (outStructSize != NULL) {
        *outStructSize = sizeof(msrcmd_t);
    }

    if (outStruct->core != inStruct->core) {
        return kIOReturnIOError;
    }

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: ReadMSR(0x%16lx) => 0x%8lx%08lx\n",
              (unsigned long)inStruct->index, (unsigned long)outStruct->hi, (unsigned long)outStruct->lo);
    #endif

    if (fCrossEndian) {
        outStruct->core = OSSwapInt32(outStruct->core);
        outStruct->index = OSSwapInt32(outStruct->index);
        outStruct->lo = OSSwapInt32(outStruct->lo);
        outStruct->hi = OSSwapInt32(outStruct->hi);
    }
    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::WriteMSRAsync(OSAsyncReference asyncRef,
                                  msrcmd_t *inStruct, msrcmd_t *outStruct,
                                  IOByteCount inStructSize,
                                  IOByteCount *outStructSize)
{
    ((void)asyncRef);
    return WriteMSR(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::WriteMSR(msrcmd_t *inStruct, msrcmd_t *outStruct,
                             IOByteCount inStructSize,
                             IOByteCount *outStructSize)
{
    ((void)inStructSize);

    if ((fProvider == NULL) || (isInactive())) {
        return kIOReturnNotAttached;
    }

    if (fCrossEndian) {
        inStruct->core = OSSwapInt32(inStruct->core);
        inStruct->index = OSSwapInt32(inStruct->index);
        inStruct->lo = OSSwapInt32(inStruct->lo);
        inStruct->hi = OSSwapInt32(inStruct->hi);
    }

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: WriteMSR(0x%16lx) = 0x%8lx%08lx\n",
              (unsigned long)inStruct->index, (unsigned long)inStruct->hi, (unsigned long)inStruct->lo);
    #endif

    MSRHelper MSRData = { inStruct, outStruct, false };

    #ifdef USE_MP_RENDEZVOUS
        mp_rendezvous(NULL, (void (*)(void *))MSRHelperFunction, NULL, (void *)&MSRData);
    #else
        mp_rendezvous_no_intrs((void (*)(void *))MSRHelperFunction, (void *)&MSRData);
    #endif

    if (outStructSize != NULL) {
        *outStructSize = sizeof(msrcmd_t);
    }

    if (outStruct->core != inStruct->core) {
        return kIOReturnIOError;
    }

    if (fCrossEndian) {
        outStruct->core = OSSwapInt32(outStruct->core);
        outStruct->index = OSSwapInt32(outStruct->index);
        outStruct->lo = OSSwapInt32(outStruct->lo);
        outStruct->hi = OSSwapInt32(outStruct->hi);
    }
    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadCpuIdAsync(OSAsyncReference asyncRef,
                                   cpuid_t * inStruct, cpuid_t * outStruct,
                                   IOByteCount inStructSize,
                                   IOByteCount * outStructSize)
{
    ((void)asyncRef);
    return ReadCpuId(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::ReadCpuId(cpuid_t * inStruct, cpuid_t * outStruct,
                              IOByteCount inStructSize,
                              IOByteCount * outStructSize)
{
    ((void)inStructSize);

    if (fProvider == NULL || isInactive()) {
        return kIOReturnNotAttached;
    }

    if (fCrossEndian) {
        inStruct->core = OSSwapInt32(inStruct->core);
        inStruct->eax = OSSwapInt32(inStruct->eax);
        inStruct->ecx = OSSwapInt32(inStruct->ecx);
    }

    CPUIDHelper cpuidData = { inStruct, outStruct};
    mp_rendezvous(NULL, (void (*)(void *))CPUIDHelperFunction, NULL,
        (void *)&cpuidData);

    *outStructSize = sizeof(cpuid_t);

    if (outStruct->core != inStruct->core)
        return kIOReturnIOError;

    if (fCrossEndian) {
        outStruct->core = OSSwapInt32(outStruct->core);
        outStruct->eax = OSSwapInt32(outStruct->eax);
        outStruct->ecx = OSSwapInt32(outStruct->ecx);
        outStruct->cpudata[0] = OSSwapInt32(outStruct->cpudata[0]);
        outStruct->cpudata[1] = OSSwapInt32(outStruct->cpudata[1]);
        outStruct->cpudata[2] = OSSwapInt32(outStruct->cpudata[2]);
        outStruct->cpudata[3] = OSSwapInt32(outStruct->cpudata[3]);
    }
    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::ReadMemAsync(OSAsyncReference asyncRef,
                                 readmem_t * inStruct, readmem_t * outStruct,
                                 IOByteCount inStructSize,
                                 IOByteCount * outStructSize)
{
    ((void)asyncRef);
    return ReadMem(inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::ReadMem(readmem_t * inStruct, readmem_t * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    ((void)inStructSize);

    if (fProvider == NULL || isInactive()) {
        return kIOReturnNotAttached;
    }

    if (fCrossEndian) {
        inStruct->core = OSSwapInt32(inStruct->core);
        inStruct->addr = OSSwapInt64(inStruct->addr);
    }

    if (cpu_number() != inStruct->core)
        return kIOReturnIOError;
    outStruct->core = inStruct->core;
    ReadMemHelper memData = { inStruct, outStruct };
    mp_rendezvous(NULL, (void (*)(void *))ReadMemHelperFunction, NULL, (void *)&memData);

    *outStructSize = sizeof(readmem_t);

    if (outStruct->core != inStruct->core)
        return kIOReturnIOError;

    if (fCrossEndian) {
        outStruct->core = OSSwapInt32(outStruct->core);
        outStruct->addr = OSSwapInt64(outStruct->addr);
        outStruct->data = OSSwapInt32(outStruct->data);
    }
    return kIOReturnSuccess;
}

IOReturn
DirectHWUserClient::Read(Parameters * inStruct, Parameters * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    return ReadWrite(kRead, inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::ReadAsync(
                            OSAsyncReference asyncRef,
                            Parameters * inStruct, Parameters * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    ((void)asyncRef);
    return ReadWrite(kRead, inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::Write(Parameters * inStruct, Parameters * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    return ReadWrite(kWrite, inStruct, outStruct, inStructSize, outStructSize);
}

IOReturn
DirectHWUserClient::WriteAsync(
                            OSAsyncReference asyncRef,
                            Parameters * inStruct, Parameters * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    ((void)asyncRef);
    return ReadWrite(kWrite, inStruct, outStruct, inStructSize, outStructSize);
}

static int pciHostBridgeCount = -1;
static IOPCIBridge * pciHostBridges[10] = {0,0,0,0,0,0,0,0,0,0};

#ifdef __ppc__
static UInt32 pciHostFlags[10] = {0,0,0,0,0,0,0,0,0,0};
enum {
    pciHostEndianChecked    = 1,
    pciHostEndianSwap       = 2,
};
#endif

void
DirectHWUserClient::GetPciHostBridges1(IOService *service, OSIterator *services)
{
    while (service) {
        IOPCIBridge *pciBridge = OSDynamicCast(IOPCIBridge, service);
        if (pciBridge) {
            DOLOG("DirectHW: Found PCI host %d: %s\n", pciHostBridgeCount, pciBridge->getName());
            pciHostBridges[pciHostBridgeCount++] = pciBridge;
        }
        else {
            OSIterator *children = service->getChildIterator(gIOServicePlane);
            IOService *child = OSDynamicCast(IOService, children->getNextObject());
            GetPciHostBridges1(child, children);
            children->release();
        }
        if (!services) {
            break;
        }
        service = OSDynamicCast(IOService, services->getNextObject());
    }
}

void
DirectHWUserClient::GetPciHostBridges(void)
{
    //DOLOG("[ DirectHW: GetPciHostBridges %d\n", pciHostBridgeCount);
    if (pciHostBridgeCount < 0) {
        pciHostBridgeCount = 0;
        IOService *device = getServiceRoot();
        GetPciHostBridges1(device, NULL);
    }
    //DOLOG("] DirectHW: GetPciHostBridges %d\n", pciHostBridgeCount);
}

IOPCIDevice *
DirectHWUserClient::FindMatching(IOService *service, IOPCIAddressSpace space, OSIterator *services)
{
    while (service) {
        IOPCIDevice *pciDevice;
        IOPCIBridge *pciBridge = NULL;

        pciDevice = OSDynamicCast(IOPCIDevice, service);
        if (pciDevice) {
            IOPCIAddressSpace regSpace;
            regSpace.bits = 0;
            regSpace.s.busNum      = pciDevice->getBusNumber();
            regSpace.s.deviceNum   = pciDevice->getDeviceNumber();
            regSpace.s.functionNum = pciDevice->getFunctionNumber();
            if (regSpace.bits == space.bits) {
                //DOLOG("DirectHW: PCIDevice %s\n", pciDevice->getName());
                pciDevice->retain();
                return pciDevice;
            }
        }
        else {
            pciBridge = OSDynamicCast(IOPCIBridge, service);
        }

        if (pciDevice || pciBridge) {
            OSIterator *children = service->getChildIterator(gIOServicePlane);
            IOService *child = OSDynamicCast(IOService, children->getNextObject());
            pciDevice = FindMatching(child, space, children);
            children->release();

            if (pciDevice) {
                return pciDevice;
            }
        }

        if (!services) {
            break;
        }
        service = OSDynamicCast(IOService, services->getNextObject());
    }
    return NULL;
}

IOReturn
DirectHWUserClient::ReadWrite(
                            uint32_t selector,
                            Parameters * inStruct, Parameters * outStruct,
                            IOByteCount inStructSize,
                            IOByteCount * outStructSize)
{
    IOReturn                   ret = kIOReturnBadArgument;
    Parameters                 * params;
    IOMemoryDescriptor         * md;
    IOMemoryMap                * map;
    void                       * vmaddr;
    IOPCIBridge                * owner = NULL;

    if (inStructSize != sizeof(Parameters)) return (kIOReturnBadArgument);
    if (outStructSize != NULL) {
        *outStructSize = sizeof(Parameters);
    }

    bcopy(inStruct, outStruct, sizeof(Parameters));
    params = outStruct;

    if (fCrossEndian) {
        params->options = OSSwapInt32(params->options);
        params->spaceType = OSSwapInt32(params->spaceType);
        params->bitWidth = OSSwapInt32(params->bitWidth);
        params->_resv = OSSwapInt32(params->_resv);
        if (kConfigSpace == params->spaceType) {
            Address address;
            address.addr64 = OSSwapInt64(params->address.addr64);
            params->address.pci.offset = address.pciswapped.offset;
            params->address.pci.function = address.pciswapped.function;
            params->address.pci.device = address.pciswapped.device;
            params->address.pci.bus = address.pciswapped.bus;
            params->address.pci.segment = address.pciswapped.segment;
            params->address.pci.reserved = address.pciswapped.reserved;
        }
        else {
            params->address.addr64 = OSSwapInt64(params->address.addr64);
        }
    }

    map = 0;
    vmaddr = 0;
    unsigned int offset = 0;
    IOPCIAddressSpace space;
    bool doSwap = false;
    bool doSkip = false;
    IOPCIDevice *pciDevice = NULL;

    if (k64BitMemorySpace == params->spaceType) {
        #ifdef __ppc__
        md = IOMemoryDescriptor::withAddress((void*)params->address.addr64, (params->bitWidth >> 3), kIODirectionOutIn);
        #else
            #if defined(KPI_10_4_0_PPC_COMPAT)
                md = IOMemoryDescriptor::withAddressRange(params->address.addr64, (params->bitWidth >> 3), kIODirectionOutIn | kIOMemoryMapperNone, NULL);
            #else
                md = IOMemoryDescriptor::withAddress((void*)params->address.addr64, (params->bitWidth >> 3), kIODirectionOutIn);
            #endif
        #endif
        if (md) {
            map = md->map();
            md->release();
        }
        if (!map) return (kIOReturnVMError);
        vmaddr = (void *)(uintptr_t) map->getAddress();
    }
    else if (kConfigSpace == params->spaceType) {
        GetPciHostBridges();

/*
        DOLOG("DirectHW: %s %04x:%02x:%02x.%01x @%02x = %llx\n",
            selector == kRead ? "Read" : selector == kWrite ? "Write" : "Unknown",
            params->address.pci.segment,
            params->address.pci.bus,
            params->address.pci.device,
            params->address.pci.function,
            params->address.pci.offset,
            params->value
        );
*/

        if (params->address.pci.segment < pciHostBridgeCount) {
            owner = pciHostBridges[params->address.pci.segment];
        }
        if (!owner) {
            DOLOG("DirectHW: %s owner not found for %04x:%02x:%02x.%01x\n",
                selector == kRead ? "Read" : selector == kWrite ? "Write" : "Uknown",
                params->address.pci.segment,
                params->address.pci.bus,
                params->address.pci.device,
                params->address.pci.function
            );
            return (kIOReturnBadArgument);
        }
        else {
            //DOLOG("DirectHW: Using PCI host: %s\n", owner->getName());
        }

        space.bits = 0;
        offset = params->address.pci.offset;
        space.s.busNum      = params->address.pci.bus;
        space.s.deviceNum   = params->address.pci.device;
        space.s.functionNum = params->address.pci.function;

#ifdef __ppc__
        if (space.s.busNum) {
            pciDevice = FindMatching(owner, space, NULL);
            if (pciDevice) {
                if (
                    (params->address.pci.offset & 0xff) >= 0x50 &&
                    (params->address.pci.offset & 0xff) < 0x54
                ) {
                    OSData *data;
                    UInt16 vendor;
                    UInt16 product;
                    if ((data = OSDynamicCast(OSData, pciDevice->getProperty("vendor-id")))) {
                        vendor = *((UInt32 *) data->getBytesNoCopy());
                        if (vendor == 0x1191) {
                            if ((data = OSDynamicCast(OSData, pciDevice->getProperty("device-id")))) {
                                product = *((UInt32 *) data->getBytesNoCopy());
                                if (product == 0x0009) {
                                    // 01:04.0 SCSI storage controller [0100]: Artop Electronic Corp ATP865 [1191:0009] (rev 03)
                                    DOLOG("DirectHW: skip read of 1191:0009 @%02x\n", params->address.pci.offset);
                                    doSkip = true;
                                } // if product
                            } // if data
                        } // if vendor
                    } // if data
                } // if offset
            } // if pcidevice
            else {
                // DEC bridge of B&W G3 causes machine check for non-existing devices
                //DOLOG("DirectHW: PCI device doesn't exist\n");
                doSkip = true;
            }
        }
        else if (
            space.s.deviceNum == 0
            && space.s.functionNum == 0
        ) {
            if (!(pciHostFlags[params->address.pci.segment] & pciHostEndianChecked)) {
                //DOLOG("DirectHW: Checking endianness of PCI host: %s\n", owner->getName());
                if (owner->configRead32(space, kIOPCIConfigVendorID) == 0x6b107400) {
                    DOLOG("DirectHW: U4 HT Bridge needs endian swapping.\n");
                    pciHostFlags[params->address.pci.segment] |= pciHostEndianSwap;
                }
                pciHostFlags[params->address.pci.segment] |= pciHostEndianChecked;
            }
            if (pciHostFlags[params->address.pci.segment] & pciHostEndianSwap) {
                doSwap = true;
                //DOLOG("DirectHW: changing offset from %x", offset);
                switch ((params->bitWidth << 4) | (offset & 3)) {
                    case  0x80: offset = (offset & ~3) | 3; break;
                    case  0x81: offset = (offset & ~3) | 2; break;
                    case  0x82: offset = (offset & ~3) | 1; break;
                    case  0x83: offset = (offset & ~3) | 0; break;
                    case 0x100: offset = (offset & ~3) | 2; break;
                    case 0x102: offset = (offset & ~3) | 0; break;
                }
                //DOLOG(" to %x\n", offset);
            }
        }
#endif

#if MAC_OS_X_VERSION_SDK >= MAC_OS_X_VERSION_10_4
        space.es.registerNumExtended = (0xF & (offset >> 8));
#endif
    }

    switch (selector) {
        case kWrite:

            if (fCrossEndian) {
                params->value = OSSwapInt64(params->value);
            }

            if (k64BitMemorySpace == params->spaceType) {
                switch (params->bitWidth) {
                    case 8:
                        *((uint8_t *) vmaddr) = params->value;
                        ret = kIOReturnSuccess;
                        break;
                    case 16:
                        *((uint16_t *) vmaddr) = params->value;
                        ret = kIOReturnSuccess;
                        break;
                    case 32:
                        *((uint32_t *) vmaddr) = static_cast<uint32_t>(params->value);
                        ret = kIOReturnSuccess;
                        break;
                    case 64:
                        *((uint64_t *) vmaddr) = params->value;
                        ret = kIOReturnSuccess;
                        break;
                    default:
                        break;
                }
            }
            else if (kConfigSpace == params->spaceType) {
                switch (params->bitWidth) {
                    case 8:
                        //DOLOG("DirectHW: Do write 8 bits (0x%02llx) using PCI host: %s\n", params->value, owner->getName());
                        if (!doSkip) owner->configWrite8(space, offset, params->value);
                        ret = kIOReturnSuccess;
                        break;
                    case 16:
                        //DOLOG("DirectHW: Do write 16 bits (0x%04llx) using PCI host: %s\n", params->value, owner->getName());
                        if (!doSkip) owner->configWrite16(space, offset, params->value);
                        ret = kIOReturnSuccess;
                        break;
                    case 32:
                        //DOLOG("DirectHW: Do write 32 bits (0x%08llx) using PCI host: %s\n", params->value, owner->getName());
                        if (!doSkip) owner->configWrite32(space, offset, static_cast<uint32_t>(params->value));
                        ret = kIOReturnSuccess;
                        break;
                    default:
                        break;
                }
            }
            break;

        case kRead:

            if (k64BitMemorySpace == params->spaceType) {
                switch (params->bitWidth) {
                    case 8:
                        params->value = *((uint8_t *) vmaddr);
                        ret = kIOReturnSuccess;
                        break;
                    case 16:
                        params->value = *((uint16_t *) vmaddr);
                        ret = kIOReturnSuccess;
                        break;
                    case 32:
                        params->value = *((uint32_t *) vmaddr);
                        ret = kIOReturnSuccess;
                        break;
                    case 64:
                        params->value = *((uint64_t *) vmaddr);
                        ret = kIOReturnSuccess;
                        break;
                    default:
                        break;
                }
            }
            else if (kConfigSpace == params->spaceType) {
                switch (params->bitWidth) {
                    case 8:
                        //DOLOG("DirectHW: Do read 8 using PCI host: %s\n", owner->getName());
                        params->value = doSkip ? (UInt8)-1 : owner->configRead8(space, offset);
                        ret = kIOReturnSuccess;
                        break;
                    case 16:
                        //DOLOG("DirectHW: Do read 16 using PCI host: %s\n", owner->getName());
                        params->value = doSkip ? (UInt16)-1 : doSwap ? OSSwapInt16(owner->configRead16(space, offset)) : owner->configRead16(space, offset);
                        ret = kIOReturnSuccess;
                        break;
                    case 32:
                        //DOLOG("DirectHW: Do read 32 using PCI host: %s\n", owner->getName());
                        params->value = doSkip ? (UInt32)-1 : doSwap ? OSSwapInt32(owner->configRead32(space, offset)) : owner->configRead32(space, offset);
                        ret = kIOReturnSuccess;
                        break;
                    default:
                        break;
                }
            }

            if (fCrossEndian) {
                params->value = OSSwapInt64(params->value);
            }
            break;

        default:
            break;
    }

    if (pciDevice) {
        //DOLOG("DirectHW: Done with pciDevice\n");
        pciDevice->release();
    }

    if (map) {
        DOLOG("DirectHW: Do map release\n");
        map->release();
    }

    return (ret);
}

IOReturn DirectHWUserClient::clientMemoryForType(UInt32 type, UInt32 *flags, IOMemoryDescriptor **memory)
{
    IOMemoryDescriptor *newmemory = NULL;

    #ifndef DEBUG_KEXT
        ((void)flags);
    #else
        DOLOG("DirectHW: clientMemoryForType(%lx, %p, %p)\n",
              (unsigned long)type, (void *)flags, (void *)memory);
    #endif

    if (type != 0) {
        DOLOG("DirectHW: Unknown mapping type %lx.\n", (unsigned long)type);

        return kIOReturnUnsupported;
    }

    if ((LastMapAddr == 0) && (LastMapSize == 0)) {
        DOLOG("DirectHW: No PrepareMap called.\n");

        return kIOReturnNotAttached;
    }

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: Mapping physical 0x%16lx[0x%lx]\n",
              (unsigned long)LastMapAddr, (unsigned long)LastMapSize);
    #endif

    if (memory != NULL) {
        newmemory = IOMemoryDescriptor::withPhysicalAddress((IOPhysicalAddress)LastMapAddr, (IOByteCount)LastMapSize, kIODirectionIn);
    }

    /* Reset mapping to zero */
    LastMapAddr = 0;
    LastMapSize = 0;

    if (newmemory == NULL) {
        DOLOG("DirectHW: Could not map memory!\n");

        return kIOReturnNotOpen;
    }

    newmemory->retain();

    if (memory != NULL) {
        *memory = newmemory;
    }

    #ifdef DEBUG_KEXT
        DOLOG("DirectHW: Mapping succeeded.\n");
    #endif

    return kIOReturnSuccess;
}
