/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2004-2024 German Aerospace Center (DLR) and others.
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
/// @file    OutputDevice_ParquetUnstructured.cpp
/// @author  Daniel Krajzewicz
/// @author  Michael Behrisch
/// @author  Jakob Erdmann
/// @author  Max Schrader
/// @author  Pranav Sateesh
/// @date    2025
///
// An output device that encapsulates an Parquet file with unstructured format
/****************************************************************************/
#include <config.h>

#ifdef HAVE_PARQUET

#include <iostream>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <utils/common/StringUtils.h>
#include <utils/common/UtilExceptions.h>
#include <utils/common/MsgHandler.h>

#include "OutputDevice_ParquetUnstructured.h"

#include <arrow/io/file.h>
#include <arrow/util/config.h>

#include <parquet/api/reader.h>
#include <parquet/exception.h>
#include <parquet/stream_reader.h>
#include <parquet/stream_writer.h>


// ===========================================================================
// method definitions
// ===========================================================================
OutputDevice_ParquetUnstructured::OutputDevice_ParquetUnstructured(const std::string& fullName)
    : OutputDevice(fullName, new ParquetUnstructuredFormatter()) {
    // Default to ZSTD compression but allow for environment variable override
    const char* compressionEnv = std::getenv("SUMO_PARQUET_COMPRESSION");
    parquet::Compression::type compression = parquet::Compression::ZSTD;
    
    if (compressionEnv != nullptr) {
        std::string compressionStr = compressionEnv;
        if (compressionStr == "SNAPPY") {
            compression = parquet::Compression::SNAPPY; // Faster but less compression
            std::cout << "Using SNAPPY compression for Parquet files" << std::endl;
        } else if (compressionStr == "NONE" || compressionStr == "UNCOMPRESSED") {
            compression = parquet::Compression::UNCOMPRESSED; // Maximum speed
            std::cout << "Using NO compression for Parquet files (maximum speed)" << std::endl;
        } else if (compressionStr == "GZIP") {
            compression = parquet::Compression::GZIP; // Better compression but slower
            std::cout << "Using GZIP compression for Parquet files" << std::endl;
        } else if (compressionStr != "ZSTD") {
            std::cerr << "Unknown compression type: " << compressionStr << ", using ZSTD" << std::endl;
        }
    }
    
    // Set the row group size from environment
    const char* rowGroupEnv = std::getenv("SUMO_PARQUET_ROWGROUP_SIZE");
    if (rowGroupEnv != nullptr) {
        try {
            int size = std::stoi(rowGroupEnv);
            if (size > 0) {
                myRowGroupSize = size;
                std::cout << "Using custom row group size: " << myRowGroupSize << std::endl;
            }
        } catch (...) {
            // Ignore invalid values
        }
    }
    
    // Set the buffer size from environment
    const char* bufferSizeEnv = std::getenv("SUMO_PARQUET_BUFFER_SIZE");
    if (bufferSizeEnv != nullptr) {
        try {
            size_t size = std::stoull(bufferSizeEnv);
            if (size > 0) {
                myBufferSize = size;
                std::cout << "Using custom buffer size: " << myBufferSize << std::endl;
            }
        } catch (...) {
            // Ignore invalid values
        }
    }
    
    // Apply the compression setting
    builder.compression(compression);
    
    // Use these writer properties for better performance
    builder.version(parquet::ParquetVersion::PARQUET_2_6);
    builder.data_page_version(parquet::ParquetDataPageVersion::V2);
    builder.enable_dictionary(); // Enable dictionary encoding for better compression
    
    // Use large write batch size for better performance
    builder.write_batch_size(10000); // Default is 1000
}

void OutputDevice_ParquetUnstructured::createNewFile() {
    if (myFile != nullptr) {
        try {
            myFile->Close();
        } catch (...) {
            // Ignore errors on close
        }
        myFile = nullptr;
    }

    auto formatter = dynamic_cast<ParquetUnstructuredFormatter*>(&this->getFormatter());
    if (formatter == nullptr) {
        throw IOError("Formatter is not a ParquetUnstructuredFormatter");
    }

    // Now that we're creating the file, finalize the schema based on buffered rows
    formatter->finalizeSchema();
    
    // If we have buffered rows but schema isn't finalized, force it
    if (!formatter->isSchemaFinalized() && formatter->getBufferedRowCount() > 0) {
        // This will build the schema from buffer
        const auto& nodeVector = formatter->getNodeVector();
        
        // If still not finalized, we can't proceed
        if (!formatter->isSchemaFinalized()) {
            std::cerr << "Warning: Cannot finalize schema for Parquet file: " << myFilename << std::endl;
            return;
        }
    }
    
    // Get the node vector for the schema
    const auto& nodeVector = formatter->getNodeVector();
    
    // Only create the file if we have a valid schema with fields
    if (!formatter->isSchemaFinalized() || formatter->getAllFields().empty() || nodeVector.empty()) {
        std::cerr << "Warning: Cannot create Parquet file with empty schema: " << myFilename << std::endl;
        return;  // Don't create file with an empty schema
    }

    try {
        // Create output file
        PARQUET_ASSIGN_OR_THROW(myFile, arrow::io::FileOutputStream::Open(myFilename));
        
        // Use ParquetUnstructuredStream which is optimized for unstructured data
        myStreamDevice = std::make_unique<ParquetUnstructuredStream>(
            parquet::ParquetFileWriter::Open(myFile, 
                std::static_pointer_cast<parquet::schema::GroupNode>(
                    parquet::schema::GroupNode::Make("schema", parquet::Repetition::REQUIRED, nodeVector)
                ), 
                builder.build()
            )
        );
        
        // Reset counters
        myRowsInCurrentGroup = 0;
    } catch (const std::exception& e) {
        std::cerr << "Error creating Parquet file: " << e.what() << std::endl;
        myFile = nullptr;
        myStreamDevice = nullptr;
    }
}

bool OutputDevice_ParquetUnstructured::closeTag(const std::string& comment) {
    UNUSED_PARAMETER(comment);
    
    auto formatter = dynamic_cast<ParquetUnstructuredFormatter*>(&this->getFormatter());
    if (formatter == nullptr) {
        return false;
    }

    // If we're at the root level, just clear the stack
    if (formatter->getDepth() < 2) {
        formatter->clearStack();
        return false;
    }

    // Let the formatter process the row
    try {
        bool result = formatter->closeTag(getOStream());
        
        // Check if we've reached the buffer threshold and schema isn't finalized yet
        if (!formatter->isSchemaFinalized() && formatter->getBufferedRowCount() >= myBufferSize) {
            // Time to finalize schema and create the file
            formatter->finalizeSchema();
            createNewFile();
            
            // Write buffered rows to the file
            std::vector<unstructured_parquet::XMLElement> rows = formatter->consumeBufferedRows();
            writeBufferedRows(rows);
        } 
        // If schema is already finalized but we haven't created the file yet
        else if (formatter->isSchemaFinalized() && myFile == nullptr) {
            createNewFile();
            
            // Write any buffered rows
            std::vector<unstructured_parquet::XMLElement> rows = formatter->consumeBufferedRows();
            writeBufferedRows(rows);
        }
        // If schema is finalized and file is created, write any buffered rows
        else if (formatter->isSchemaFinalized() && myFile != nullptr && formatter->getBufferedRowCount() > 0) {
            std::vector<unstructured_parquet::XMLElement> rows = formatter->consumeBufferedRows();
            writeBufferedRows(rows);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Error in OutputDevice_ParquetUnstructured::closeTag: " << e.what() << std::endl;
        return true; // Return true to prevent calling code from breaking
    }
}

OutputDevice_ParquetUnstructured::~OutputDevice_ParquetUnstructured() {
    try {
        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Get the formatter
        auto formatter = dynamic_cast<ParquetUnstructuredFormatter*>(&this->getFormatter());
        if (formatter == nullptr) {
            return;
        }
        
        // First check if we have any buffered rows
        if (formatter->getBufferedRowCount() > 0) {
            // Try to build the schema from buffered rows first
            formatter->finalizeSchema();
            
            // If we have rows but fields are still empty after trying to build the schema,
            // force schema generation with default types (fallback to strings)
            if (formatter->getAllFields().empty()) {
                // Force a build of the node vector from buffer, which should populate fields
                const auto& nodeVector = formatter->getNodeVector();
                
                // If still empty after trying to build, there's nothing we can do
                if (formatter->getAllFields().empty() || nodeVector.empty()) {
                    std::cerr << "Warning: Not writing Parquet file with empty schema to " 
                          << myFilename << std::endl;
                    return;
                }
            }
            
            // At this point, we should have a valid schema, create the file
            if (myFile == nullptr) {
                try {
                    createNewFile();
                    
                    // If file creation still failed, return
                    if (myFile == nullptr) {
                        std::cerr << "Warning: Could not create Parquet file: " << myFilename << std::endl;
                        return;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error creating Parquet file " << myFilename << ": " << e.what() << std::endl;
                    return;
                }
            }
            
            // Get all buffered rows
            std::vector<unstructured_parquet::XMLElement> rows = formatter->consumeBufferedRows();
            
            // Write each row with error handling
            auto parquetStream = dynamic_cast<ParquetUnstructuredStream*>(myStreamDevice.get());
            if (parquetStream != nullptr) {
                int rowsWritten = 0;
                int rowsSkipped = 0;

                // Determine which elements to write based on the SUMO output type
                // For most outputs, we want to write the leaf elements (those that contain actual data)
                // but not structural elements like "timestep"
                std::vector<unstructured_parquet::XMLElement> elementsToWrite;
                for (auto& row : rows) {
                    // Skip known container or structural elements that shouldn't be written directly
                    if (row.getName() == "timestep" || row.getName() == "interval" || 
                        row.getName() == "meandata" || row.getName() == "data" ||
                        row.getName() == "summary" || row.getName() == "step" ||
                        row.getName() == "fcd-export") {
                        continue;
                    }
                    
                    // Include all data elements
                    elementsToWrite.push_back(std::move(row));
                }
                
                // Get the schema information once
                std::set<std::string> knownFields = formatter->getAllFields();
                int columnCount = parquetStream->getColumnCount();
                
                // Write the selected elements
                for (auto& row : elementsToWrite) {
                    try {
                        // Check if this row has attributes that aren't in the schema
                        // This could happen if new attributes are added during simulation
                        bool hasNewAttributes = false;
                        
                        for (const auto& attr : row.getAttributes()) {
                            if (knownFields.find(attr->getName()) == knownFields.end()) {
                                hasNewAttributes = true;
                                std::cerr << "Found new attribute not in schema: " << attr->getName() << std::endl;
                                // Add to schema for next file creation if needed
                                formatter->addFieldToSchema(attr->getName(), attr->getParquetType(), attr->getConvertedType());
                            }
                        }
                        
                        // Get the list of field names in the schema order
                        std::set<std::string> processedAttrNames;
                        std::vector<const unstructured_parquet::AttributeBase*> orderedAttributes;
                        
                        // First collect all attributes in the schema order
                        if (columnCount > 0) {
                            // Pre-allocate the attributes array with nulls
                            orderedAttributes.resize(columnCount, nullptr);
                            
                            // Get all attributes as a map for faster lookup
                            std::map<std::string, const unstructured_parquet::AttributeBase*> attrMap;
                            for (const auto& attr : row.getAttributes()) {
                                // Only include attributes that are in the current schema
                                if (knownFields.find(attr->getName()) != knownFields.end()) {
                                    attrMap[attr->getName()] = attr.get();
                                }
                            }
                            
                            // Order attributes according to schema column order
                            int colIndex = 0;
                            for (const std::string& colName : parquetStream->getColumnNames()) {
                                if (colIndex >= columnCount) {
                                    // Safety check to avoid out-of-bounds access
                                    break;
                                }
                                
                                auto it = attrMap.find(colName);
                                if (it != attrMap.end()) {
                                    orderedAttributes[colIndex] = it->second;
                                    processedAttrNames.insert(colName);
                                }
                                colIndex++;
                            }
                        }
                        
                        // Write attributes in order with explicit column positioning
                        // Only loop up to the columnCount to avoid out-of-bounds issues
                        for (int i = 0; i < columnCount && i < static_cast<int>(orderedAttributes.size()); i++) {
                            try {
                                // Set column position explicitly before writing
                                int initialPos = i;
                                parquetStream->setColumnIndex(initialPos);
                                
                                if (orderedAttributes[i] != nullptr) {
                                    orderedAttributes[i]->print(*myStreamDevice);
                                } else {
                                    // Write null for missing attribute
                                    parquetStream->writeNullOrDefault(initialPos);
                                }
                                
                                // If the parquet stream's column index changed during write, reset it
                                // to where it should be for the next attribute
                                int currentPos = parquetStream->getCurrentColumnIndex();
                                if (currentPos != initialPos && currentPos != initialPos + 1) {
                                    // Something unexpected happened - fix the position
                                    parquetStream->setColumnIndex(initialPos + 1);
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "Warning: Failed to write column " << i 
                                      << " to Parquet file: " << e.what() << std::endl;
                                // Continue with next column instead of failing the entire row
                            }
                        }
                        
                        // Don't try to adjust the index at the end - endLine() will handle remaining columns
                        // and terminate the row correctly
                        
                        // End the row
                        try {
                            myStreamDevice->endLine();
                            myRowsInCurrentGroup++;
                            rowsWritten++;
                            
                            // Create a new row group if needed
                            if (myRowsInCurrentGroup >= myRowGroupSize) {
                                parquetStream->endRowGroup();
                                myRowsInCurrentGroup = 0;
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "Error ending row in Parquet file: " << e.what() << std::endl;
                            rowsSkipped++;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error writing row to Parquet file: " << e.what() << std::endl;
                        rowsSkipped++;
                    }
                }
                
                // End the final row group if needed
                if (myRowsInCurrentGroup > 0) {
                    try {
                        parquetStream->endRowGroup();
                    } catch (const std::exception& e) {
                        std::cerr << "Error ending final row group in Parquet file: " << e.what() << std::endl;
                    }
                }
                
                // If rows were skipped, log a summary
                if (rowsSkipped > 0) {
                    std::cerr << "Warning: Skipped " << rowsSkipped << " out of " << (rowsWritten + rowsSkipped) 
                          << " rows when writing to " << myFilename << std::endl;
                }
                
                if (rowsWritten > 0) {
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    double seconds = duration / 1000.0;
                    double rowsPerSecond = rowsWritten / seconds;
                    
                    std::cout << "Successfully wrote " << rowsWritten << " rows to " << myFilename 
                              << " in " << seconds << " seconds (" 
                              << static_cast<int>(rowsPerSecond) << " rows/sec)" << std::endl;
                }
            }
        }

        // Clean up resources
        try {
            myStreamDevice.reset();
            
            if (myFile) {
                [[maybe_unused]] arrow::Status status = myFile->Close();
            }
        } catch (const std::exception& e) {
            std::cerr << "Error closing Parquet file " << myFilename << ": " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error during Parquet file cleanup: " << e.what() << std::endl;
    }
}

// Create a dummy stream that will discard all output for safety
static OStreamDevice s_dummyStream(std::make_unique<std::ostringstream>().release());

StreamDevice& OutputDevice_ParquetUnstructured::getOStream() {
    // If the stream doesn't exist yet, return a dummy stream that discards output
    if (myStreamDevice == nullptr) {
        return s_dummyStream;
    }
    return *myStreamDevice;
}

// Add this new helper method after createNewFile() method
void OutputDevice_ParquetUnstructured::writeBufferedRows(std::vector<unstructured_parquet::XMLElement>& rows) {
    if (rows.empty() || myFile == nullptr) {
        return;
    }
    
    auto parquetStream = dynamic_cast<ParquetUnstructuredStream*>(myStreamDevice.get());
    if (parquetStream == nullptr) {
        return;
    }
    
    auto formatter = dynamic_cast<ParquetUnstructuredFormatter*>(&this->getFormatter());
    if (formatter == nullptr) {
        return;
    }
    
    // Get all fields in schema
    std::set<std::string> knownFields = formatter->getAllFields();
    int columnCount = parquetStream->getColumnCount();
    int rowsWritten = 0;
    int rowsSkipped = 0;
    
    // Determine which elements to write (similar logic as in the destructor)
    std::vector<unstructured_parquet::XMLElement> elementsToWrite;
    for (auto& row : rows) {
        // Skip known container elements
        if (row.getName() == "timestep" || row.getName() == "interval" || 
            row.getName() == "meandata" || row.getName() == "data" ||
            row.getName() == "summary" || row.getName() == "step" ||
            row.getName() == "fcd-export") {
            continue;
        }
        
        // Include all data elements
        elementsToWrite.push_back(std::move(row));
    }
    
    // Write the selected elements
    for (auto& row : elementsToWrite) {
        try {
            // Process this row similar to how we do in the destructor
            std::set<std::string> processedAttrNames;
            std::vector<const unstructured_parquet::AttributeBase*> orderedAttributes;
            
            // First collect all attributes in the schema order
            if (columnCount > 0) {
                // Pre-allocate the attributes array with nulls
                orderedAttributes.resize(columnCount, nullptr);
                
                // Get all attributes as a map for faster lookup
                std::map<std::string, const unstructured_parquet::AttributeBase*> attrMap;
                for (const auto& attr : row.getAttributes()) {
                    // Only include attributes that are in the current schema
                    if (knownFields.find(attr->getName()) != knownFields.end()) {
                        attrMap[attr->getName()] = attr.get();
                    }
                }
                
                // Order attributes according to schema column order
                int colIndex = 0;
                for (const std::string& colName : parquetStream->getColumnNames()) {
                    if (colIndex >= columnCount) {
                        break;
                    }
                    
                    auto it = attrMap.find(colName);
                    if (it != attrMap.end()) {
                        orderedAttributes[colIndex] = it->second;
                        processedAttrNames.insert(colName);
                    }
                    colIndex++;
                }
            }
            
            // Write attributes in order with explicit column positioning
            for (int i = 0; i < columnCount && i < static_cast<int>(orderedAttributes.size()); i++) {
                try {
                    int initialPos = i;
                    parquetStream->setColumnIndex(initialPos);
                    
                    if (orderedAttributes[i] != nullptr) {
                        orderedAttributes[i]->print(*myStreamDevice);
                    } else {
                        parquetStream->writeNullOrDefault(initialPos);
                    }
                    
                    int currentPos = parquetStream->getCurrentColumnIndex();
                    if (currentPos != initialPos && currentPos != initialPos + 1) {
                        parquetStream->setColumnIndex(initialPos + 1);
                    }
                } catch (const std::exception& e) {
                    // Continue with next column
                }
            }
            
            // End the row
            try {
                myStreamDevice->endLine();
                myRowsInCurrentGroup++;
                rowsWritten++;
                
                // Create a new row group if needed
                if (myRowsInCurrentGroup >= myRowGroupSize) {
                    parquetStream->endRowGroup();
                    myRowsInCurrentGroup = 0;
                }
            } catch (const std::exception& e) {
                rowsSkipped++;
            }
        } catch (const std::exception& e) {
            rowsSkipped++;
        }
    }
    
    // Clear the buffer now that we've written it
    rows.clear();
}

#endif
/****************************************************************************/