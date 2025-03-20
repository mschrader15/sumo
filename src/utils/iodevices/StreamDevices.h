#pragma once
#include <config.h>
#include <fstream> 
#include <memory>
#include <string>

#ifdef HAVE_PARQUET
#include <arrow/io/file.h>
#include <arrow/util/config.h>

#include <parquet/api/reader.h>
#include <parquet/exception.h>
#include <parquet/stream_writer.h>
#endif


class StreamDevice {
public:

    /// @brief The type of the stream
    enum Type {
        OSTREAM, // std::ostream (or std::ofstream)
        COUT, // std::cout
        PARQUET // parquet::StreamWriter
    };

    // create a constructor that a type and raw write access
    StreamDevice(Type type, bool access) : rawWriteAccess(access), myType(type)  {};
    // create a default constructor
    StreamDevice() = default;

    /// @brief Destructor
    virtual ~StreamDevice() = default;

    /// @brief is the stream ok
    /// @return true if the stream is ok
    virtual bool ok() = 0;

    /// @brief flush the stream
    /// @return this 
    virtual StreamDevice& flush() = 0;

    /// @brief close the stream
    virtual void close() = 0;

    /// @brief is the stream good
    /// @return true if the stream is good
    virtual bool good() = 0;

    /// @brief read the stream into a string
    /// @return the string 
    virtual std::string str() = 0;

    /// @brief  set the precision
    /// @param precision 
    virtual void setPrecision(int precision) = 0;

    /// @brief get the precision
    /// @return the precision
    virtual int precision() = 0;

    /// @brief implement a stream operator
    virtual operator std::ostream& () = 0;

    /// @brief write a string to the stream
    /// @param s the string to write
    virtual void str(const std::string& s) = 0;

    /// @brief write an endline to the stream
    /// @return this
    virtual StreamDevice& endLine() = 0;

    /// @brief set the output stream flags
    /// @param flags the flags to set
    virtual void setOSFlags(std::ios_base::fmtflags flags) = 0;

    /// @brief get the type of the stream
    virtual Type type() const {
        return myType;
    };

    /// @brief allow raw output
    bool allowRaw() const {
        return rawWriteAccess;
    }

    /// @brief define the behavior of a cast to std::ostream
    virtual std::ostream& getOStream() {
        throw std::runtime_error("Not implemented");
    }

protected:
    /// @brief allow raw write access
    bool rawWriteAccess = false;
    /// @brief the type of the stream
    Type myType = Type::OSTREAM;
        
};

class OStreamDevice : public StreamDevice {
public:

    // write a constructor that takes a std::ofstream
    OStreamDevice(std::ofstream* stream) : StreamDevice(Type::OSTREAM, true), myStream(std::move(stream)) {}
    OStreamDevice(std::ostream* stream) : StreamDevice(Type::OSTREAM, true), myStream(std::move(stream)) {}
    OStreamDevice(std::ofstream stream) : StreamDevice(Type::OSTREAM, true), myStream(new std::ofstream(std::move(stream))) {}
    OStreamDevice(std::basic_ostream<char> stream) : StreamDevice(Type::OSTREAM, true), myStream(&stream) {}

    virtual ~OStreamDevice() override  = default;

    bool ok() override {
        return myStream->good();
    }

    StreamDevice& flush() override {
        myStream->flush();
        return *this;
    }

    void close() override {
        myStream->flush();
    }

    template <typename T>
    StreamDevice& print(const T& t) {
        (*myStream) << t;
        return *this;
    }

    void setPrecision(int precision) override {
        (*myStream) << std::setprecision(precision);
    }

    void setOSFlags(std::ios_base::fmtflags flags) override {
        myStream->setf(flags);
    }

    int precision() override {
        return (int)myStream->precision();
    }

    bool good() override {
        return myStream->good();
    }

    std::string str() override {
        // Try casting to ostringstream
        if (auto* oss_ptr = dynamic_cast<std::ostringstream*>(myStream.get())) {
            return oss_ptr->str();
        }
        
        // Try casting to stringstream
        if (auto* ss_ptr = dynamic_cast<std::stringstream*>(myStream.get())) {
            return ss_ptr->str();
        }
        
        // If it's neither, we need to use a more general approach
        std::ostringstream oss;
        oss << myStream->rdbuf();
        return oss.str();
    }

    void str(const std::string& s) override {
        (*myStream) << s;
    }

    operator std::ostream& () override {
        return *myStream;
    }

    StreamDevice& endLine() override {
        (*myStream) << std::endl;
        return *this;
    }

    // get the type of the stream
    Type type() const override {
        return Type::OSTREAM;
    }

    std::ostream& getOStream() override {
        return *myStream;
    }

private:
    std::unique_ptr<std::ostream> myStream;
}; // Add the missing semicolon here


class COUTStreamDevice : public StreamDevice {
public:

    // write a constructor that takes a std::ofstream
    COUTStreamDevice() : StreamDevice(Type::COUT, true), myStream(std::cout) {};
    COUTStreamDevice(std::ostream& stream) : StreamDevice(Type::COUT, true), myStream(stream) {};

    virtual ~COUTStreamDevice() override = default;

    bool ok() override {
        return myStream.good();
    }

    StreamDevice& flush() override {
        myStream.flush();
        return *this;
    }

    void close() override {
        (void)(this->flush());
    }

    template <typename T>
    StreamDevice& print(const T& t) {
        myStream << t;
        return *this;
    }

    void setPrecision(int precision) override {
        myStream << std::setprecision(precision);
    }

    void setOSFlags(std::ios_base::fmtflags flags) override {
        myStream.setf(flags);
    }

    int precision() override {
        return static_cast<int>(myStream.precision());
    }

    bool good() override {
        return myStream.good();
    }

    std::string str() override {
        return "";
    }

    void str(const std::string& s) override {
        myStream << s;
    }

    operator std::ostream& () override {
        return myStream;
    }

    StreamDevice& endLine() override {
        myStream << std::endl;
        return *this;
    }

    std::ostream& getOStream() override {
        return myStream;
    }

private:

    std::ostream& myStream;

}; // Add the missing semicolon here

class ParquetStream : public StreamDevice {

#ifdef HAVE_PARQUET

public:

    ParquetStream(std::unique_ptr<parquet::ParquetFileWriter> file) : StreamDevice(Type::PARQUET, false) {
        // For StreamWriter, we need to move the unique_ptr into it
        // Note: this approach means the StreamWriter will own the ParquetFileWriter
        // Save schema before we move the file into the StreamWriter
        auto schema = file->schema();
        numColumns = schema->num_columns();
        
        // Create the StreamWriter with the file
        myStream = std::unique_ptr<parquet::StreamWriter>(new parquet::StreamWriter(std::move(file)));
        
        // Initialize column index
        currentColumnIndex = 0;
        
        // Build a map of column names to types
        for (int i = 0; i < numColumns; i++) {
            auto col = schema->Column(i);
            columnTypes[col->name()] = col->physical_type();
            columnNames.push_back(col->name());
        }
    };

    virtual ~ParquetStream() = default;

    bool ok() override {
        return true;
    }

    bool good() override {
        // check that the stream is not null
        return myStream != nullptr;
    }

    StreamDevice& flush() override {
        // do nothing
        return *this;
    }

    void close() override {
        try {
            if (myStream) {
                // Finish any incomplete row
                if (currentColumnIndex > 0 && currentColumnIndex < numColumns) {
                    while (currentColumnIndex < numColumns) {
                        writeNullOrDefault(currentColumnIndex);
                        currentColumnIndex++;
                    }
                    myStream->EndRow();
                }
                
                // End the row group
                myStream->EndRowGroup();
                
                // Release the stream writer
                myStream.reset();
            }
            
            // The file writer will be released automatically when the unique_ptr is destroyed
        } catch (const std::exception& e) {
            std::cerr << "Error closing ParquetStream: " << e.what() << std::endl;
        }
    }

    void setPrecision(int precision) override {
        UNUSED_PARAMETER(precision);
    }

    std::string str() override {
        return "";
    }
    
    // Method to handle writing values with proper conversion based on target column type
    template <typename T>
    void print(const T& t) {
        if (!myStream) {
            std::cerr << "Error: Stream not initialized" << std::endl;
            return;
        }
        
        if (currentColumnIndex >= numColumns) {
            std::cerr << "Error: Attempting to write beyond the schema column count (" 
                     << currentColumnIndex << " >= " << numColumns << ")" << std::endl;
            return;
        }
        
        try {
            std::string currentColumnName = "";
            if (currentColumnIndex < columnNames.size()) {
                currentColumnName = columnNames[currentColumnIndex];
            }
            
            auto columnType = parquet::Type::BYTE_ARRAY; // Default to string
            if (!currentColumnName.empty()) {
                auto it = columnTypes.find(currentColumnName);
                if (it != columnTypes.end()) {
                    columnType = it->second;
                }
            }
            
            // Remember the column index before writing
            int initialColumnIndex = currentColumnIndex;
            
            // Handle conversion based on destination column type
            switch (columnType) {
                case parquet::Type::BOOLEAN: {
                    bool value = convertToBool(t);
                    (*myStream) << value;
                    break;
                }
                case parquet::Type::INT32: {
                    int32_t value = convertToInt32(t);
                    (*myStream) << value;
                    break;
                }
                case parquet::Type::INT64: {
                    int64_t value = convertToInt64(t);
                    (*myStream) << value;
                    break;
                }
                case parquet::Type::FLOAT: {
                    float value = convertToFloat(t);
                    (*myStream) << value;
                    break;
                }
                case parquet::Type::DOUBLE: {
                    double value = convertToDouble(t);
                    (*myStream) << value;
                    break;
                }
                case parquet::Type::BYTE_ARRAY:
                case parquet::Type::FIXED_LEN_BYTE_ARRAY:
                default: {
                    // Default to string for any other type
                    std::string value = convertToString(t);
                    (*myStream) << value;
                    break;
                }
            }
            
            // Only increment if we're not already at the last column
            // This now ensures we only increment once, regardless of whether the Parquet API incremented internally
            if (initialColumnIndex == currentColumnIndex && initialColumnIndex < numColumns - 1) {
                currentColumnIndex++;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error writing value to column " << currentColumnIndex;
            if (currentColumnIndex < columnNames.size()) {
                std::cerr << " (" << columnNames[currentColumnIndex] << ")";
            }
            std::cerr << ": " << e.what() << std::endl;
            
            try {
                // Try writing a null or default value based on column type
                writeNullOrDefault(currentColumnIndex);
                
                // Only increment if we're not already at the last column
                if (currentColumnIndex < numColumns - 1) {
                    currentColumnIndex++;
                }
            } catch (...) {
                // Last resort - skip this column, but only if we're not at the last one
                if (currentColumnIndex < numColumns - 1) {
                    currentColumnIndex++;
                }
            }
        }
    }

    void setOSFlags(std::ios_base::fmtflags flags) override {UNUSED_PARAMETER(flags);}

    operator std::ostream& () override {
        throw std::runtime_error("Not implemented");
    }

    void str(const std::string& s) override {
        UNUSED_PARAMETER(s);
        throw std::runtime_error("Not implemented");
    };

    StreamDevice& endLine() override {
        try {
            // Only try to fill columns if we have a valid stream
            if (myStream) {
                // Safety check - ensure we don't exceed the valid column range
                if (currentColumnIndex >= numColumns) {
                    currentColumnIndex = (numColumns > 0) ? (numColumns - 1) : 0;
                }

                // If we haven't written all columns for this row, fill with nulls/defaults
                // But be careful to stay within valid column indices
                while (currentColumnIndex < numColumns - 1) {
                    try {
                        // Safely write this column's null value with index protection
                        int currentCol = currentColumnIndex;
                        writeNullOrDefault(currentCol);
                        
                        // Only increment if our position hasn't changed (meaning no auto-increment happened)
                        if (currentColumnIndex == currentCol) {
                            currentColumnIndex++;
                        }
                    } catch (const std::exception& e) {
                        // If an error occurs, log it but still try to continue
                        std::cerr << "Error filling missing column " << currentColumnIndex << ": " << e.what() << std::endl;
                        
                        // Safely increment
                        if (currentColumnIndex < numColumns - 1) {
                            currentColumnIndex++;
                        } else {
                            break;
                        }
                    }
                }
                
                // Handle the last column specially if we're not already there
                if (numColumns > 0 && currentColumnIndex == numColumns - 1) {
                    try {
                        writeNullOrDefault(currentColumnIndex);
                    } catch (const std::exception& e) {
                        std::cerr << "Error filling last column " << currentColumnIndex << ": " << e.what() << std::endl;
                    }
                }
                
                // End the row and reset the column index
                try {
                    myStream->EndRow();
                } catch (const std::exception& e) {
                    std::cerr << "Error ending row: " << e.what() << std::endl;
                }
                
                currentColumnIndex = 0; // Reset column index for next row
            }
        } catch (const std::exception& e) {
            std::cerr << "Error ending row in Parquet: " << e.what() << std::endl;
            currentColumnIndex = 0; // Reset column index for next row
        }
        return *this;
    }

    // Add the endRowGroup method
    void endRowGroup() {
        try {
            if (myStream) {
                // End any incomplete row first
                if (currentColumnIndex > 0 && currentColumnIndex < numColumns) {
                    // Fill remaining columns with nulls, but be careful with column indices
                    while (currentColumnIndex < numColumns - 1) {
                        try {
                            int currentCol = currentColumnIndex; // Save current position
                            writeNullOrDefault(currentCol);
                            
                            // Only increment if position hasn't changed
                            if (currentColumnIndex == currentCol) {
                                currentColumnIndex++;
                            }
                        } catch (const std::exception& e) {
                            // If an error occurs, log but continue
                            std::cerr << "Error filling null in endRowGroup for column " << currentColumnIndex << ": " << e.what() << std::endl;
                            
                            // Safe increment
                            if (currentColumnIndex < numColumns - 1) {
                                currentColumnIndex++;
                            } else {
                                break;
                            }
                        }
                    }
                    
                    // Handle the last column separately and safely
                    if (numColumns > 0 && currentColumnIndex == numColumns - 1) {
                        try {
                            writeNullOrDefault(currentColumnIndex);
                        } catch (const std::exception& e) {
                            std::cerr << "Error filling null for last column in endRowGroup: " << e.what() << std::endl;
                        }
                    }
                    
                    try {
                        myStream->EndRow();
                    } catch (const std::exception& e) {
                        std::cerr << "Error ending row in endRowGroup: " << e.what() << std::endl;
                    }
                    
                    currentColumnIndex = 0;
                }
                
                // Now end the row group
                try {
                    myStream->EndRowGroup();
                } catch (const std::exception& e) {
                    std::cerr << "Error in parquet EndRowGroup: " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error ending row group in Parquet: " << e.what() << std::endl;
            currentColumnIndex = 0; // Reset column index for safety
        }
    }

    // get the type of the stream
    Type type() const override {
        return Type::PARQUET;
    }

    int precision() override {
        return 0;
    }
    
    // Get the count of columns in the schema
    int getColumnCount() const {
        return numColumns;
    }
    
    // Get the names of columns in schema order
    const std::vector<std::string>& getColumnNames() const {
        return columnNames;
    }
    
    // Get the current column index
    int getCurrentColumnIndex() const {
        return currentColumnIndex;
    }
    
    // Set the current column index for the next write
    void setColumnIndex(int index) {
        if (index >= 0 && index < numColumns) {
            currentColumnIndex = index;
        } else {
            // If index is out of bounds, set to the last column or 0 if no columns
            currentColumnIndex = (numColumns > 0) ? (numColumns - 1) : 0;
            std::cerr << "Warning: Column index " << index << " out of bounds for " << numColumns << " columns - adjusted to " << currentColumnIndex << std::endl;
        }
    }
    
    // Helper method to write a null or default value based on column type
    void writeNullOrDefault(int colIndex) {
        if (colIndex < 0 || colIndex >= numColumns) {
            // Skip writing if the column index is out of bounds
            std::cerr << "Warning: Skipping write for out-of-bounds column index " << colIndex 
                      << " (max: " << (numColumns - 1) << ")" << std::endl;
            return;
        }
        
        try {
            // Get the column type from our stored map rather than accessing schema again
            parquet::Type::type colType = parquet::Type::BYTE_ARRAY; // Default type
            
            if (colIndex < columnNames.size()) {
                const std::string& colName = columnNames[colIndex];
                auto it = columnTypes.find(colName);
                if (it != columnTypes.end()) {
                    colType = it->second;
                }
            }
            
            // Explicitly set column position before writing to ensure correct positioning
            // This is needed because some Parquet operations might increment the index internally
            int originalIndex = currentColumnIndex;
            currentColumnIndex = colIndex;
            
            switch (colType) {
                case parquet::Type::BOOLEAN:
                    (*myStream) << false;
                    break;
                case parquet::Type::INT32:
                    (*myStream) << 0;
                    break;
                case parquet::Type::INT64:
                    (*myStream) << (int64_t)0;
                    break;
                case parquet::Type::FLOAT:
                    (*myStream) << 0.0f;
                    break;
                case parquet::Type::DOUBLE:
                    (*myStream) << 0.0;
                    break;
                case parquet::Type::BYTE_ARRAY:
                case parquet::Type::FIXED_LEN_BYTE_ARRAY:
                default:
                    (*myStream) << "";
                    break;
            }
            
            // After writing, ensure the index hasn't incremented unexpectedly
            // If streaming operations auto-incremented the index, we need to
            // restore it to avoid skipping columns
            if (currentColumnIndex != colIndex) {
                // Reset back to the original index + 1 (natural progression)
                currentColumnIndex = colIndex;
            }
        } catch (const std::exception& e) {
            // Don't rethrow as we want to continue even if one column fails
        }
    }

private:
    std::unique_ptr<parquet::StreamWriter> myStream;
    int numColumns{0};
    int currentColumnIndex{0};
    std::map<std::string, parquet::Type::type> columnTypes;
    std::vector<std::string> columnNames;

    // Helper conversion methods
    template <typename T>
    bool convertToBool(const T& value) {
        if constexpr (std::is_same_v<T, bool>) {
            return value;
        } else if constexpr (std::is_integral_v<T>) {
            return value != 0;
        } else if constexpr (std::is_floating_point_v<T>) {
            return value != 0.0;
        } else if constexpr (std::is_same_v<T, std::string> || 
                           std::is_same_v<T, const char*> ||
                           std::is_same_v<T, std::string_view>) {
            std::string str = std::string(value);
            return !str.empty() && str != "0" && str != "false" && str != "False" && str != "FALSE";
        } else {
            return !convertToString(value).empty();
        }
    }
    
    template <typename T>
    int32_t convertToInt32(const T& value) {
        if constexpr (std::is_integral_v<T> && sizeof(T) <= 4) {
            return static_cast<int32_t>(value);
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<int32_t>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<int32_t>(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? 1 : 0;
        } else if constexpr (std::is_same_v<T, std::string> || 
                           std::is_same_v<T, const char*> ||
                           std::is_same_v<T, std::string_view>) {
            try {
                return std::stoi(std::string(value));
            } catch (...) {
                return 0;
            }
        } else {
            try {
                return std::stoi(convertToString(value));
            } catch (...) {
                return 0;
            }
        }
    }
    
    template <typename T>
    int64_t convertToInt64(const T& value) {
        if constexpr (std::is_integral_v<T>) {
            return static_cast<int64_t>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<int64_t>(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? 1 : 0;
        } else if constexpr (std::is_same_v<T, std::string> || 
                           std::is_same_v<T, const char*> ||
                           std::is_same_v<T, std::string_view>) {
            try {
                return std::stoll(std::string(value));
            } catch (...) {
                return 0;
            }
        } else {
            try {
                return std::stoll(convertToString(value));
            } catch (...) {
                return 0;
            }
        }
    }
    
    template <typename T>
    float convertToFloat(const T& value) {
        if constexpr (std::is_floating_point_v<T>) {
            return static_cast<float>(value);
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<float>(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? 1.0f : 0.0f;
        } else if constexpr (std::is_same_v<T, std::string> || 
                           std::is_same_v<T, const char*> ||
                           std::is_same_v<T, std::string_view>) {
            try {
                return std::stof(std::string(value));
            } catch (...) {
                return 0.0f;
            }
        } else {
            try {
                return std::stof(convertToString(value));
            } catch (...) {
                return 0.0f;
            }
        }
    }
    
    template <typename T>
    double convertToDouble(const T& value) {
        if constexpr (std::is_floating_point_v<T>) {
            return static_cast<double>(value);
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<double>(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? 1.0 : 0.0;
        } else if constexpr (std::is_same_v<T, std::string> || 
                           std::is_same_v<T, const char*> ||
                           std::is_same_v<T, std::string_view>) {
            try {
                return std::stod(std::string(value));
            } catch (...) {
                return 0.0;
            }
        } else {
            try {
                return std::stod(convertToString(value));
            } catch (...) {
                return 0.0;
            }
        }
    }
    
    template <typename T>
    std::string convertToString(const T& value) {
        try {
            return ::toString(value);
        } catch (...) {
            return "";
        }
    }

#endif
};

// implement a templated stream operator. The base class does nothing
template <typename T>
StreamDevice& operator<<(StreamDevice& stream, const T& t) {
    try {
        switch (stream.type()) {
        case StreamDevice::Type::OSTREAM:
            static_cast<OStreamDevice*>(&stream)->print(t);
            break;
        case StreamDevice::Type::COUT:
            static_cast<COUTStreamDevice*>(&stream)->print(t);
            break;
        case StreamDevice::Type::PARQUET:
#ifdef HAVE_PARQUET
            static_cast<ParquetStream*>(&stream)->print(t);
#else
            throw std::runtime_error("Parquet not supported in this build");
#endif
            break;
        default:
            // assert that this does not happen
            throw std::runtime_error("Unknown stream type in StreamDevice");
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in stream operator<<: " << e.what() << std::endl;
    }
    return stream;
}