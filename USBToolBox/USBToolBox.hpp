//
//  PortPopulator.hpp
//  USBToolBox
//
//  Created by Dhinak G on 6/1/20.
//  Copyright © 2020-2021 Dhinak G. All rights reserved.
//

#ifndef USBToolBox_hpp
#define USBToolBox_hpp

#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <pexpert/pexpert.h>

#define super IOService
#define PRODUCT_NAME USBToolBox

#ifdef DEBUG
#define TRACELOG kprintf
#else
#define TRACELOG IOLog
#endif

#define SYSTEMLOG(format, args...) do { TRACELOG("USBToolBox: " format "\n", ##args); } while (false)
#define SYSTEMLOGPROV(format, args...) do { TRACELOG("USBToolBox: %s: " format "\n", pciDevice->getName(), ##args); } while (false)

// Debug log
#ifdef DEBUG
#define DEBUGLOG(format, args...) SYSTEMLOG(format, ##args)
#define DEBUGLOGPROV(format, args...) SYSTEMLOGPROV(format, ##args)
#else
#define DEBUGLOG(format, args...)
#define DEBUGLOGPROV(format, args...)
#endif


class USBToolBox : public IOService {
    OSDeclareDefaultStructors(USBToolBox)

    static constexpr const int PARENT_PATH_LENGTH = 512;
    static constexpr const int MODEL_NAME_LENGTH = 16;

    static constexpr const uint64_t MAX_WAIT = 180;
    static constexpr const uint64_t DEFAULT_WAIT = 1;
    static constexpr const uint64_t NANOSECONDS = 1000000000;
    // Boot args
    static constexpr const char *bootargOff {"-utboff"};              // Disable the kext
    static constexpr const char *bootargAcpiOff {"-utbacpioff"};      // Disable RHUB removal from ACPI plane
    static constexpr const char *bootargAppleOff {"-utbappleoff"};    // Disable existing `ports` and `ports-count` removal
    static constexpr const char *bootargMapOff {"-utbmapoff"};        // Disable the map
    static constexpr const char *bootargMatchWait {"utbwait"};        // Modify waitForMatchingService delay
    
    // Properties
    static constexpr const char *propertyOff {"utb-off"};
    static constexpr const char *propertyAcpiOff {"utb-acpi-off"};
    static constexpr const char *propertyAppleOff {"utb-apple-off"};
    static constexpr const char *propertyMapOff {"utb-map-off"};
    static constexpr const char *propertyMatchWait {"utb-wait"};

    static constexpr const char *versionString =
#ifdef VERBOSE
    "VBS-"
#elif defined DEBUG
    "DBG-"
#else
    "REL-"
#endif
     __DATE__ "-" __TIME__;
public:
    virtual IOService* probe(IOService* provider, SInt32* score) override;
    virtual bool start(IOService* provider) override;
private:
    uint64_t matchWait = DEFAULT_WAIT; // Default timeout in seconds

    IORegistryEntry* pciDevice = NULL;
    IORegistryEntry* controllerInstance = NULL;
    
    void deleteProperty(IORegistryEntry* provider, const char* property);
    OSDictionary* createMatchingDictionary();
    bool matchingCallback(OSDictionary* matchingDict, IOService* newService, IONotifier* notifier);
    static bool _matchingCallback(void* matchingDict, void* refCon, IOService* newService, IONotifier* notifier);
    IORegistryEntry* getControllerViaMatching();
    IORegistryEntry* getControllerViaIteration();
    OSObject* fixMapForTahoe(OSObject* object);
    void mergeProperties(IORegistryEntry* instance = NULL);
    void removeACPIPorts();
    
    static bool checkClassHierarchy(IORegistryEntry* provider, const char* className);
    static inline bool checkKernelArgument(const char* arg) {
        uint32_t value = 0;
        return PE_parse_boot_argn(arg, &value, sizeof(value));
    };
};

#endif /* USBToolBox_hpp */
