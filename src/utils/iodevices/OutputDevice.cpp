/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2004-2025 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    OutputDevice.cpp
/// @author  Daniel Krajzewicz
/// @author  Jakob Erdmann
/// @author  Michael Behrisch
/// @date    2004
///
// Static storage of an output device and its base (abstract) implementation
/****************************************************************************/
#include <config.h>

#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#endif
#include "OutputDevice.h"
#include "OutputDevice_File.h"
#include "OutputDevice_COUT.h"
#include "OutputDevice_CERR.h"
#include "OutputDevice_Network.h"
#include "OutputDevice_Parquet.h"
#include "OutputDevice_ParquetUnstructured.h"
#include "PlainXMLFormatter.h"
#include <utils/common/StringUtils.h>
#include <utils/common/UtilExceptions.h>
#include <utils/common/FileHelpers.h>
#include <utils/common/ToString.h>
#include <utils/common/MsgHandler.h>
#include <utils/options/OptionsCont.h>
#include <utils/options/OptionsIO.h>
#include <utils/iodevices/OutputDevice_String.h>
#ifdef HAVE_PARQUET
#include <utils/iodevices/OutputDevice_Parquet.h>
#include <utils/iodevices/OutputDevice_ParquetUnstructured.h>
#endif

#ifdef HAVE_S3
#include <arrow/filesystem/s3fs.h>
#include "S3Utils.h"
#endif


// ===========================================================================
// static member definitions
// ===========================================================================
std::map<std::string, OutputDevice*> OutputDevice::myOutputDevices;
int OutputDevice::myPrevConsoleCP = -1;
int OutputDevice::myPrecision = OUTPUT_ACCURACY;

#ifdef HAVE_S3
// Initialize the shared S3 variables
std::mutex sumo::s3::s3FinalizationMutex;
bool sumo::s3::s3WasInitialized = false;
#endif


// ===========================================================================
// static method definitions
// ===========================================================================
OutputDevice&
OutputDevice::getDevice(const std::string& name, bool usePrefix) {
#ifdef WIN32
    // fix the windows console output on first call
    if (myPrevConsoleCP == -1) {
        myPrevConsoleCP = GetConsoleOutputCP();
        SetConsoleOutputCP(CP_UTF8);
    }
#endif
    // check whether the device has already been aqcuired
    if (myOutputDevices.find(name) != myOutputDevices.end()) {
        return *myOutputDevices[name];
    }
    // build the device
    OutputDevice* dev = nullptr;
    // check whether the device shall print to stdout
    if (name == "stdout") {
        dev = OutputDevice_COUT::getDevice();
    } else if (name == "stderr") {
        dev = OutputDevice_CERR::getDevice();
    }  
    else {
        std::string name2 = (name == "nul" || name == "NUL") ? "/dev/null" : name;
        if (usePrefix && OptionsCont::getOptions().isSet("output-prefix") && name2 != "/dev/null") {
            std::string prefix = OptionsCont::getOptions().getString("output-prefix");
            const std::string::size_type metaTimeIndex = prefix.find("TIME");
            if (metaTimeIndex != std::string::npos) {
                const time_t rawtime = std::chrono::system_clock::to_time_t(OptionsIO::getLoadTime());
                char buffer [80];
                struct tm* timeinfo = localtime(&rawtime);
                strftime(buffer, 80, "%Y-%m-%d-%H-%M-%S", timeinfo);
                prefix.replace(metaTimeIndex, 4, buffer);
            }
            name2 = FileHelpers::prependToLastPathComponent(prefix, name);
        }
        name2 = StringUtils::substituteEnvironment(name2, &OptionsIO::getLoadTime());
        // check the file extension
        const auto file_ext = FileHelpers::getExtension(name);
        const int len = (int)name.length();
        if (file_ext == ".parquet" || file_ext == ".prq") {
#ifdef HAVE_PARQUET
            // Check if this is an FCD output file
            bool isFCDOutput = false;
            // Check if we're handling the fcd-output option
            if (OptionsCont::getOptions().isSet("fcd-output")) {
                const std::string fcdOutput = OptionsCont::getOptions().getString("fcd-output");
                if (name == fcdOutput) {
                    isFCDOutput = true;
                }
            }
            // Also check if the filename contains "fcd"
            if (!isFCDOutput && (name.find("fcd") != std::string::npos || name2.find("fcd") != std::string::npos)) {
                isFCDOutput = true;
            }
            
            if (isFCDOutput) {
                // Use the structured Parquet writer for FCD outputs
                std::cout << "Creating structured Parquet writer for FCD output: " << name2 << std::endl;
                dev = new OutputDevice_Parquet(name2);
            } else {
                // Use the unstructured Parquet writer for all other outputs
                std::cout << "Creating unstructured Parquet writer for output: " << name2 << std::endl;
                dev = new OutputDevice_ParquetUnstructured(name2);
            }
#else
            throw IOError(TL("Parquet output is not supported in this build."));
#endif
        }
        else {
            dev = new OutputDevice_File(name2, len > 3 && FileHelpers::getExtension(name) == ".gz");
        }
    }
    // todo: extract this to a class method? (b.c. Parquet doesn't have an iostream)
    dev->setPrecision();
    dev->setOSFlags(std::ios::fixed);
    myOutputDevices[name] = dev;
    return *dev;
}


bool
OutputDevice::createDeviceByOption(const std::string& optionName,
                                   const std::string& rootElement,
                                   const std::string& schemaFile) {
    if (!OptionsCont::getOptions().isSet(optionName)) {
        return false;
    }
    OutputDevice& dev = OutputDevice::getDevice(OptionsCont::getOptions().getString(optionName));
    if (rootElement != "") {
        dev.writeXMLHeader(rootElement, schemaFile);
    }
    return true;
}


OutputDevice&
OutputDevice::getDeviceByOption(const std::string& optionName) {
    std::string devName = OptionsCont::getOptions().getString(optionName);
    if (myOutputDevices.find(devName) == myOutputDevices.end()) {
        throw InvalidArgument("Output device '" + devName + "' for option '" + optionName + "' has not been created.");
    }
    return OutputDevice::getDevice(devName);
}


void
OutputDevice::flushAll() {
    for (auto item : myOutputDevices) {
        item.second->flush();
    }
}


void
OutputDevice::closeAll(bool keepErrorRetrievers) {
    std::vector<OutputDevice*> errorDevices;
    std::vector<OutputDevice*> nonErrorDevices;
    for (std::map<std::string, OutputDevice*>::iterator i = myOutputDevices.begin(); i != myOutputDevices.end(); ++i) {
        if (MsgHandler::getErrorInstance()->isRetriever(i->second)) {
            errorDevices.push_back(i->second);
        } else {
            nonErrorDevices.push_back(i->second);
        }
    }
    for (OutputDevice* const dev : nonErrorDevices) {
        try {
            dev->close();
        } catch (const IOError& e) {
            WRITE_ERROR(TL("Error on closing output devices."));
            WRITE_ERROR(e.what());
        }
    }
    if (!keepErrorRetrievers) {
        for (OutputDevice* const dev : errorDevices) {
            try {
                dev->close();
            } catch (const IOError& e) {
                std::cerr << "Error on closing error output devices." << std::endl;
                std::cerr << e.what() << std::endl;
            }
        }
#ifdef WIN32
        if (myPrevConsoleCP != -1) {
            SetConsoleOutputCP(myPrevConsoleCP);
        }
#endif
    }
}


std::string
OutputDevice::realString(const double v, const int precision) {
    std::ostringstream oss;
    if (v == 0) {
        return "0";
    }
    if (fabs(v) < pow(10., -precision)) {
        oss.setf(std::ios::scientific, std::ios::floatfield);
    } else {
        oss.setf(std::ios::fixed, std::ios::floatfield);     // use decimal format
        oss.setf(std::ios::showpoint);    // print decimal point
        oss << std::setprecision(precision);
    }
    oss << v;
    return oss.str();
}


// ===========================================================================
// member method definitions
// ===========================================================================
OutputDevice::OutputDevice(const int defaultIndentation, const std::string& filename) :
    myFilename(filename), myFormatter(new PlainXMLFormatter(defaultIndentation)) {
}

OutputDevice::OutputDevice(const std::string& filename,  OutputFormatter* formatter) :
    myFilename(filename), myFormatter(formatter) {
}


bool
OutputDevice::ok() {
    return getOStream().good();
}


const std::string&
OutputDevice::getFilename() {
    return myFilename;
}

void
OutputDevice::close() {
    while (closeTag()) {}
    for (std::map<std::string, OutputDevice*>::iterator i = myOutputDevices.begin(); i != myOutputDevices.end(); ++i) {
        if (i->second == this) {
            myOutputDevices.erase(i);
            break;
        }
    }
    MsgHandler::removeRetrieverFromAllInstances(this);
    delete this;
}


void
OutputDevice::setPrecision(int precision) {
    getOStream().setPrecision(precision);
}


int
OutputDevice::precision() {
    return getOStream().precision();
}


bool
OutputDevice::writeXMLHeader(const std::string& rootElement,
                             const std::string& schemaFile,
                             std::map<SumoXMLAttr, std::string> attrs,
                             bool includeConfig) {
    if (schemaFile != "") {
        attrs[SUMO_ATTR_XMLNS] = "http://www.w3.org/2001/XMLSchema-instance";
        attrs[SUMO_ATTR_SCHEMA_LOCATION] = "http://sumo.dlr.de/xsd/" + schemaFile;
    }
    return myFormatter->writeXMLHeader(getOStream(), rootElement, attrs, includeConfig);
}


OutputDevice&
OutputDevice::openTag(const std::string& xmlElement) {
    myFormatter->openTag(getOStream(), xmlElement);
    return *this;
}


OutputDevice&
OutputDevice::openTag(const SumoXMLTag& xmlElement) {
    myFormatter->openTag(getOStream(), xmlElement);
    return *this;
}


bool
OutputDevice::closeTag(const std::string& comment) {
    if (myFormatter->closeTag(getOStream(), comment)) {
        postWriteHook();
        return true;
    }
    return false;
}


void
OutputDevice::postWriteHook() {}


void
OutputDevice::inform(const std::string& msg, const bool progress) {
    if (progress) {
        getOStream() << msg;
    } else {
        getOStream() << msg << '\n';
    }
    postWriteHook();
}


void
OutputDevice::finalizeGlobalOutput() {
    // close all devices
    std::vector<OutputDevice*> devices;
    for (auto& item : myOutputDevices) {
        devices.push_back(item.second);
    }
    // clear map to avoid closing being called several times
    myOutputDevices.clear();
    for (auto device : devices) {
        try {
            //device->close();
            delete device;
        } catch (const IOError& e) {
            WRITE_ERROR("Error on closing output devices. " + std::string(e.what()));
        }
    }
    
#ifdef HAVE_S3
    // Finalize S3 once at application shutdown
    try {
        std::lock_guard<std::mutex> lock(sumo::s3::s3FinalizationMutex);
        if (sumo::s3::s3WasInitialized) {
            arrow::Status status = arrow::fs::FinalizeS3();
            if (!status.ok()) {
                std::cerr << "Warning: Error finalizing S3: " << status.ToString() << std::endl;
            } else {
                std::cout << "S3 resources finalized successfully" << std::endl;
            }
            sumo::s3::s3WasInitialized = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during global S3 finalization: " << e.what() << std::endl;
    }
#endif
}


/****************************************************************************/
