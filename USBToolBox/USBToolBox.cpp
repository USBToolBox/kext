//
//  USBToolBox.cpp
//  USBToolBox
//
//  Created by Dhinak G on 6/1/20.
//  Copyright © 2020-2021 Dhinak G. All rights reserved.
//

#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>
#include <libkern/version.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/c++/OSMetaClass.h>

#include "USBToolBox.hpp"

#define super IOService
#define PRODUCT_NAME USBToolBox


#define checkProperty(a) (this->pciDevice && this->pciDevice->getProperty(a)) || (this->controllerInstance && this->controllerInstance->getProperty(a)) || getProperty(a)

OSDefineMetaClassAndStructors(USBToolBox, IOService)

OSDictionary* USBToolBox::createMatchingDictionary() {
    char pciDevicePath[PARENT_PATH_LENGTH] = { 0 };
    int pciDevicePathLength = sizeof(pciDevicePath);
    
    bool pciDevicePathStatus = pciDevice->getPath(pciDevicePath, &pciDevicePathLength, gIOServicePlane);
    
    if (!pciDevicePathStatus) {
        SYSTEMLOGPROV("Failed to get PCI device path!");
        return NULL;
    } else {
        DEBUGLOGPROV("Path is %s, length %d", pciDevicePath, pciDevicePathLength);
    }
    
    OSString* pciDevicePathString = OSString::withCString(pciDevicePath);
    
    if (!pciDevicePathString) {
        SYSTEMLOGPROV("Failed to create OSString of PCI device path!");
        return NULL;
    }
    
    const OSSymbol* pathMatchKey = OSSymbol::withCString("IOPathMatch");
    
    if (!pathMatchKey) {
        SYSTEMLOGPROV("Failed to create path match key!");
        OSSafeReleaseNULL(pciDevicePathString);
        return NULL;
    }
    
    const OSObject* parentMatchObjects[] {pciDevicePathString};
    const OSSymbol* parentMatchSymbols[] {pathMatchKey};
    
    OSDictionary* parentMatch = OSDictionary::withObjects(parentMatchObjects, parentMatchSymbols, 1);
    
    OSSafeReleaseNULL(pathMatchKey);
    OSSafeReleaseNULL(pciDevicePathString);
    
    if (!parentMatch) {
        SYSTEMLOGPROV("Failed to create parent match dict!");
        return NULL;
    }
    
    OSDictionary* matchingDict = serviceMatching("AppleUSBHostController");
    
    if (!matchingDict) {
        SYSTEMLOGPROV("Failed to create matching dict!");
        OSSafeReleaseNULL(parentMatch);
        return NULL;
    }
    
    bool setParentMatchStatus = matchingDict->setObject("IOParentMatch", parentMatch);
    
    OSSafeReleaseNULL(parentMatch);
    
    if (!setParentMatchStatus) {
        SYSTEMLOGPROV("Failed to set parent match!");
        OSSafeReleaseNULL(matchingDict);
        return NULL;
    }
    
    char modelNameArray[MODEL_NAME_LENGTH] = { 0 };
    // bool modelNameStatus = PEGetProductName(modelNameArray, MODEL_NAME_LENGTH); Big Sur+
    bool modelNameStatus = PEGetModelName(modelNameArray, MODEL_NAME_LENGTH);
    
    if (!modelNameStatus) {
        SYSTEMLOGPROV("Failed to get model name!");
        OSSafeReleaseNULL(matchingDict);
        return NULL;
    } else {
        DEBUGLOGPROV("Name is %s", modelNameArray);
    }
    
    OSString* modelName = OSString::withCString(modelNameArray);
    
    if (!modelName) {
        SYSTEMLOGPROV("Could not create OSString with model name!");
        OSSafeReleaseNULL(matchingDict);
        return NULL;
    }
    
    bool setModelStatus = matchingDict->setObject("model", modelName);
    
    modelName->release();
    
    if (!setModelStatus) {
        SYSTEMLOGPROV("Failed to set model!");
        OSSafeReleaseNULL(matchingDict);
        return NULL;
    }
    
    return matchingDict;
}

IORegistryEntry* USBToolBox::getControllerViaMatching() {
    PE_parse_boot_argn(bootargMatchWait, &matchWait, sizeof(matchWait));
    
    if (OSObject* matchWaitProperty = pciDevice->getProperty(propertyMatchWait)) {
        if (OSNumber* matchWaitNumber = OSDynamicCast(OSNumber, matchWaitProperty)) {
            matchWait = matchWaitNumber->unsigned64BitValue();
        } else {
            DEBUGLOGPROV("Provider match wait property is incorrect type");
        }
    }
    
    if (OSObject* matchWaitProperty = getProperty(propertyMatchWait)) {
        if (OSNumber* matchWaitNumber = OSDynamicCast(OSNumber, matchWaitProperty)) {
            matchWait = matchWaitNumber->unsigned64BitValue();
        } else {
            DEBUGLOGPROV("Match wait property is incorrect type");
        }
    }
    
    if (matchWait > MAX_WAIT) {
        SYSTEMLOGPROV("Match wait is too high, ignoring");
        matchWait = DEFAULT_WAIT;
    }
    
    OSDictionary* matchingDict = createMatchingDictionary();
    
    if (!matchingDict) {
        SYSTEMLOGPROV("Failed to create matching dict!");
        return NULL;
    }
    
    DEBUGLOGPROV("Starting waitForMatchingService");
    IOService* controller = IOService::waitForMatchingService(matchingDict, matchWait * NANOSECONDS);
    
    OSSafeReleaseNULL(matchingDict);
    
    if (controller) {
        DEBUGLOGPROV("waitForMatchingService successful");
    } else {
        SYSTEMLOGPROV("waitForMatchingService failed or timed out");
    }
    return controller;
}

bool USBToolBox::checkClassHierarchy(IORegistryEntry* entry, const char* className) {
    const OSSymbol* classSymbol = OSSymbol::withCString(className);
    
    if (!classSymbol) {
        DEBUGLOG("Failed to create class symbol for %s!", className);
        return false;
    }
    
    const OSMetaClass* entryMetaclass = entry->getMetaClass();
    while (entryMetaclass) {
        if (entryMetaclass->getClassNameSymbol() == classSymbol) {
            OSSafeReleaseNULL(classSymbol);
            return true;
        }
        entryMetaclass = entryMetaclass->getSuperClass();
    };
    OSSafeReleaseNULL(classSymbol);
    return false;
}

IORegistryEntry* USBToolBox::getControllerViaIteration() {
    OSIterator* iterator = pciDevice->getChildIterator(gIOServicePlane);
    if (!iterator) {
        SYSTEMLOGPROV("Failed to create IOService iterator!");
        return NULL;
    }
    
    IORegistryEntry* controller = NULL;
    
    DEBUGLOGPROV("Starting iteration over IOService plane");
    while (IORegistryEntry* entry = OSDynamicCast(IORegistryEntry, iterator->getNextObject())) {
        // DEBUGLOGPROV("Object name is %s with class %s", entry->getName(), entry->getMetaClass()->getClassName());
        if (checkClassHierarchy(entry, "AppleUSBHostController")) {
            // This is it
            DEBUGLOGPROV("%s is AppleUSBHostController instance", entry->getName());
            // Retain before we exit iterator to prevent any races.
            entry->retain();
            controller = entry;
            break;
        }
    }
    DEBUGLOGPROV("Iteration over IOService plane finished");
    OSSafeReleaseNULL(iterator);
    
    return controller;
}

void USBToolBox::deleteProperty(IORegistryEntry* entry, const char* property) {
    if (entry->getProperty(property)) {
        DEBUGLOGPROV("%s exists, removing", property);
        entry->removeProperty(property);
        DEBUGLOGPROV("removed %s", property);
    } else {
        DEBUGLOGPROV("%s is null, not removing", property);
    }
}

void USBToolBox::mergeProperties(IORegistryEntry* instance) {
    if (instance) {
        this->controllerInstance = instance;
    }
    DEBUGLOGPROV("Merging properties");
    
    if (!checkClassHierarchy(this->pciDevice, "IOUSBHostDevice")) {
        if (!(checkKernelArgument(bootargAppleOff) || checkProperty(propertyAppleOff))) {
            DEBUGLOGPROV("Removing any preexisting ports");
            deleteProperty(this->controllerInstance, "ports");
            deleteProperty(this->controllerInstance, "port-count");
        } else {
            SYSTEMLOGPROV("Apple off specified, not removing any preexisting ports");
        }
    }
    
    if (!(checkKernelArgument(bootargMapOff) || checkProperty(propertyMapOff))) {
        OSDictionary* properties = NULL;
        if ((properties = OSDynamicCast(OSDictionary, getProperty("IOProviderMergeProperties")))) {
            DEBUGLOGPROV("Applying map");
        } else if ((properties = OSDynamicCast(OSDictionary, this->pciDevice->getProperty("IOProviderMergeProperties")))) {
            DEBUGLOGPROV("Applying map from provider");
        }
        
        if (properties) {
            if (OSCollectionIterator* propertyIterator = OSCollectionIterator::withCollection(properties)) {
                DEBUGLOGPROV("Starting iteration over properties");
                while (OSSymbol* key = OSDynamicCast(OSSymbol, propertyIterator->getNextObject())) {
                    //DEBUGLOGPROV("Applied property %s", key->getCStringNoCopy());
                    this->controllerInstance->setProperty(key, properties->getObject(key));
                }
                DEBUGLOGPROV("Successfully applied map");
                OSSafeReleaseNULL(propertyIterator);
            } else {
                SYSTEMLOGPROV("Failed to create property iterator!");
            }
        } else {
            DEBUGLOGPROV("No properties to apply");
        }
        
    } else {
        SYSTEMLOGPROV("Map disabled, continuing");
    }
    this->controllerInstance->release();
}

void USBToolBox::removeACPIPorts() {
    if (!(checkKernelArgument(bootargAcpiOff) || checkProperty(propertyAcpiOff))) {
        const IORegistryPlane* acpiPlane = IORegistryEntry::getPlane("IOACPIPlane");
        
        if (!acpiPlane) {
            SYSTEMLOGPROV("Could not get ACPI plane!");
            return;
        }
        
        IORegistryEntry* acpiEntry = OSDynamicCast(IORegistryEntry, pciDevice->getProperty("acpi-device"));
        
        if (acpiEntry) {
            if (OSIterator* acpiIterator = acpiEntry->getChildIterator(acpiPlane)) {
                DEBUGLOGPROV("Starting iteration over ACPI plane");
                while (IORegistryEntry* entry = OSDynamicCast(IORegistryEntry, acpiIterator->getNextObject())) {
                    DEBUGLOGPROV("Object name is %s, detatching", entry->getName());
                    entry->detachAll(acpiPlane);
                }
                DEBUGLOGPROV("Iteration over ACPI plane finished");
                acpiEntry->setProperty("ACPI removed by USBToolBox", true);
                OSSafeReleaseNULL(acpiIterator);
            } else {
                SYSTEMLOGPROV("Failed to create ACPI iterator!");
            }
        } else {
            SYSTEMLOGPROV("Failed to get ACPI entry or none available");
        }
    } else {
        SYSTEMLOGPROV("ACPI fixup disabled, continuing");
    }
}

IOService* USBToolBox::probe(IOService* provider, SInt32* score) {
    DEBUGLOG("%s: probe start", provider->getName());
    
    if (OSDynamicCast(IOPCIDevice, provider)) {
        // We are attached to the PCI device
        this->pciDevice = provider;
    } else if (checkClassHierarchy(provider, "AppleUSBHostController") || checkClassHierarchy(provider, "IOUSBHostDevice")) {
        // We are already attached to the instance
        this->controllerInstance = provider;
        this->controllerInstance->retain();
        this->pciDevice = this->controllerInstance->getParentEntry(gIOServicePlane); // Should not be released by the caller.
        if (!(this->pciDevice)) {
            DEBUGLOG("%s: No parent PCI device!", provider->getName());
            return NULL;
        }
    } else {
        DEBUGLOG("%s: Unknown provider class %s!", provider->getName(), provider->getMetaClass()->getClassName());
        return NULL;
    }
    
    if (checkKernelArgument(bootargOff) || checkProperty(propertyOff)) {
        SYSTEMLOGPROV("disable argument specified, exiting");
        return NULL;
    }
    
    this->pciDevice->setProperty("UTB Version", versionString);
    
    if (!checkClassHierarchy(this->pciDevice, "IOUSBHostDevice")
        removeACPIPorts();
    
    if (!(this->controllerInstance)) {
        this->controllerInstance = getControllerViaMatching();
    }
    if (!(this->controllerInstance)) {
        DEBUGLOGPROV("Failed to obtain via matching in probe");
    } else {
        mergeProperties();
        DEBUGLOGPROV("Probe early finish");
        return NULL;
    }
    DEBUGLOGPROV("Probe end");
    return super::probe(provider, score);
}

bool USBToolBox::matchingCallback(OSDictionary* matchingDict, IOService* newService, IONotifier* notifier) {
    DEBUGLOGPROV("controller callback start");
    newService->retain();
    // These actions are synchronized with invocations of the notification handler, so removing a notification request will guarantee the handler is not being executed.
    notifier->remove();
    mergeProperties(newService);
    // addMatchingNotification does not consume a reference on the matching dictionary when the notification is removed, unlike addNotification.
    // Should never be null, but better safe than sorry.
    OSSafeReleaseNULL(matchingDict);
    
    DEBUGLOGPROV("controller callback end");
    terminate();
    return true;
}

bool USBToolBox::_matchingCallback(void* target, void* matchingDict, IOService* newService, IONotifier* notifier) {
    return static_cast<USBToolBox*>(target)->matchingCallback(static_cast<OSDictionary*>(matchingDict), newService, notifier);
}

bool USBToolBox::start(IOService *provider) {
    DEBUGLOGPROV("start start");
    // If we're here we couldn't get the instance in probe
    DEBUGLOGPROV("Trying to obtain via matching");
    this->controllerInstance = getControllerViaMatching();
    
    if (!(this->controllerInstance)) {
        DEBUGLOGPROV("Failed to obtain via matching, falling back to iteration");
        this->controllerInstance = getControllerViaIteration();
    }
    
    if (!(this->controllerInstance)) {
        DEBUGLOGPROV("Failed to obtain via iteration, falling back to notification");
        OSDictionary* matchingDict = createMatchingDictionary();
        
        if (!matchingDict) {
            SYSTEMLOGPROV("Failed to create matching dict!");
            return false;
        }
        if (!gIOPublishNotification) {
            SYSTEMLOGPROV("IOPublishNotification null!");
            OSSafeReleaseNULL(matchingDict);
            return false;
        }
        // static IONotifier* addMatchingNotification(const OSSymbol* type, OSDictionary* matching, IOServiceMatchingNotificationHandler handler, void* target, void* ref = NULL, SInt32 priority = 0)
        IONotifier* notifierStatus = addMatchingNotification(gIOMatchedNotification, matchingDict, _matchingCallback, this, matchingDict);
        DEBUGLOGPROV("Installed controller notifier status: %s", notifierStatus ? "successful" : "failed");
    } else {
        mergeProperties();
    }
    DEBUGLOGPROV("start exit");
    return super::start(provider);
}

void USBToolBox::free() {
    super::free();
}
