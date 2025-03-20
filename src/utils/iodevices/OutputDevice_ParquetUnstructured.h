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
/// @file    OutputDevice_ParquetUnstructured.h
/// @author  Daniel Krajzewicz
/// @author  Michael Behrisch
/// @author  Jakob Erdmann
/// @author  Max Schrader
/// @date    2024
///
// An output device that encapsulates a Parquet file with unstructured row-based format
/****************************************************************************/
#pragma once

#include <config.h>

#ifdef HAVE_PARQUET

#include <iostream>
#include "OutputDevice.h"
#include "ParquetUnstructuredFormatter.h"

#include <arrow/io/file.h>
#include <arrow/util/config.h>
#include <parquet/exception.h>
#include <parquet/stream_reader.h>
#include <parquet/stream_writer.h>



/**
 * @class OutputDevice_ParquetUnstructured
 * @brief An output device that encapsulates an parquet stream writer
 *
 * Please note that the device is responsible for the stream and deletes
 *  it (it should not be deleted elsewhere).
 *
 * Performance can be tuned using the following environment variables:
 * - SUMO_PARQUET_COMPRESSION: compression type (ZSTD, SNAPPY, GZIP, NONE)
 * - SUMO_PARQUET_ROWGROUP_SIZE: number of rows per row group (default: 1000000)
 * - SUMO_PARQUET_BUFFER_SIZE: buffer size for schema detection (default: 10000000)
 */
class OutputDevice_ParquetUnstructured : public OutputDevice {
public:
    /** @brief Constructor
     * @param[in] fullName The name of the output file to use
     * @exception IOError Should not be thrown by this implementation
     */
    OutputDevice_ParquetUnstructured(const std::string& fullName);

    /// @brief Destructor
    ~OutputDevice_ParquetUnstructured() override;

    /** @brief implements the close tag logic. This is where the file is first opened and the schema is created.
     * This exploits the fact that for *most* SUMO files, all the fields are present at the first close tag event.
     */
    bool closeTag(const std::string& comment) override;

    /** @brief writes a line feed if applicable. overriden from the base class to do nothing
     */
    void lf() {};

    // null the setPrecision method
    void setPrecision(int precision) override {
        UNUSED_PARAMETER(precision);
    };

    void setOSFlags(std::ios_base::fmtflags flags) override {
        UNUSED_PARAMETER(flags);
    };

    /** @brief Set the row group size for Parquet file writing
     * @param[in] size The number of rows per row group
     */
    void setRowGroupSize(int size) {
        if (size > 0) {
            myRowGroupSize = size;
        }
    }

    /** @brief Set the buffer size for schema determination
     * @param[in] size The number of rows to buffer before finalizing schema
     */
    void setBufferSize(size_t size) {
        if (size > 0) {
            myBufferSize = size;
        }
    }

    /** @brief Set the output type for this device
     * @param[in] type The type of output (e.g., "edge", "lane")
     */
    void setOutputType(const std::string& type) {
        auto formatter = dynamic_cast<ParquetUnstructuredFormatter*>(&this->getFormatter());
        if (formatter != nullptr) {
            formatter->setOutputType(type);
        }
    }

    /** @brief Get the output type
     * @return The type of output
     */
    const std::string& getOutputType() const {
        // Need to use const_cast since the base class doesn't have a const getFormatter method
        auto formatter = dynamic_cast<const ParquetUnstructuredFormatter*>(const_cast<OutputFormatter*>(&const_cast<OutputDevice_ParquetUnstructured*>(this)->getFormatter()));
        if (formatter != nullptr) {
            return formatter->getOutputType();
        }
        static const std::string empty;
        return empty;
    }

protected:

    /// @brief Returns whether the output device is a parquet
    OutputWriterType getType() const override {
        return OutputWriterType::PARQUET;
    }

    /// @brief Get a safe stream device that won't crash if the file isn't open yet
    StreamDevice& getOStream() override;

    /// do I allow optional attributes
    bool allowOptionalAttributes = false;

private:
    /// The wrapped ofstream
    std::shared_ptr<arrow::io::FileOutputStream> myFile = nullptr;
    // the builder for the writer properties
    parquet::WriterProperties::Builder builder;
    // the schema
    std::shared_ptr<parquet::schema::GroupNode> schema;

    /// am I redirecting to /dev/null
    bool myAmNull = false;

    /// my full name
    std::string myFullName;

    /// @brief Number of rows in a row group
    int myRowGroupSize = 1000000;
    
    /// @brief Current number of rows in the current row group
    int myRowsInCurrentGroup = 0;
    
    /// @brief Number of rows to buffer before finalizing schema
    size_t myBufferSize = 10000000;

    parquet::schema::NodeVector myNodeVector;

    /** @brief Create a new Parquet file with the current schema
     */
    void createNewFile();
};

#endif // HAVE_PARQUET