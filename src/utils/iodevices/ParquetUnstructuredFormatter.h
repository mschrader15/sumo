/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.dev/sumo
// Copyright (C) 2012-2024 German Aerospace Center (DLR) and others.
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
/// @file    ParquetUnstructuredFormatter.h
/// @author  Daniel Krajzewicz
/// @author  Michael Behrisch
/// @author  Jakob Erdmann
/// @author  Max Schrader
/// @date    2024
///
// Output formatter for unstructured Parquet output
/****************************************************************************/
#pragma once
#include <config.h>

#ifdef HAVE_PARQUET
// parquet-cpp
#include <arrow/io/file.h>
#include <arrow/util/config.h>

#include <parquet/api/reader.h>
#include <parquet/exception.h>
#include <parquet/stream_writer.h>

#include "OutputFormatter.h"
#include <utils/common/ToString.h>
#include "StreamDevices.h"

// Create a namespace to avoid symbol redefinitions with regular ParquetFormatter
namespace unstructured_parquet {

#define PARQUET_TESTING

// Helper function to determine if a type is a fixed-length character array
template <typename T>
struct is_fixed_char_array : std::false_type {};

template <std::size_t N>
struct is_fixed_char_array<char[N]> : std::true_type {};

// Helper template for the static_assert
template <typename T>
constexpr bool always_false = false;

// Overloaded function for different types
template <typename T>
void AppendField(parquet::schema::NodeVector& fields, const T& val, const std::string& field_name) {
    try {
        UNUSED_PARAMETER(val);
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, const char*> || std::is_same_v<T, std::string_view>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
                parquet::ConvertedType::UTF8));
        }
        else if constexpr (std::is_same_v<T, char>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::FIXED_LEN_BYTE_ARRAY,
                parquet::ConvertedType::NONE, 1));
        }
        else if constexpr (is_fixed_char_array<T>::value) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::FIXED_LEN_BYTE_ARRAY,
                parquet::ConvertedType::NONE, sizeof(T)));
        }
        else if constexpr (std::is_same_v<T, int8_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::INT_8));
        }
        else if constexpr (std::is_same_v<T, uint8_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::UINT_8));
        }
        else if constexpr (std::is_same_v<T, int16_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::INT_16));
        }
        else if constexpr (std::is_same_v<T, uint16_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::UINT_16));
        }
        else if constexpr (std::is_same_v<T, int32_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::INT_32));
        }
        else if constexpr (std::is_same_v<T, uint32_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT32,
                parquet::ConvertedType::UINT_32));
        }
        else if constexpr (std::is_same_v<T, int64_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT64,
                parquet::ConvertedType::INT_64));
        }
        else if constexpr (std::is_same_v<T, uint64_t>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT64,
                parquet::ConvertedType::UINT_64));
        }
        else if constexpr (std::is_same_v<T, float>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::FLOAT,
                parquet::ConvertedType::NONE));
        }
        else if constexpr (std::is_same_v<T, double>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::DOUBLE,
                parquet::ConvertedType::NONE));
        }
        else if constexpr (std::is_same_v<T, bool>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::BOOLEAN,
                parquet::ConvertedType::NONE));
        }
        else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT64,
                parquet::ConvertedType::TIMESTAMP_MICROS));
        }
        else if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::INT64,
                parquet::ConvertedType::TIMESTAMP_MILLIS));
        }
        else {
            // For any unhandled type, fall back to UTF8 string
            fields.push_back(parquet::schema::PrimitiveNode::Make(
                field_name, parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
                parquet::ConvertedType::UTF8));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating schema field '" << field_name << "': " << e.what() << std::endl;
        // Fallback to string type for safety
        fields.push_back(parquet::schema::PrimitiveNode::Make(
            field_name, parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
            parquet::ConvertedType::UTF8));
    }
}

// ===========================================================================
// class definitions
// ===========================================================================
/**
 * @class TypedAttribute
 * @brief A class to represent an attribute with a specific type
 *
 * This class is used to represent an attribute of an XML XMLElement with a specific type.
 */
// Base class
class AttributeBase {
public:
    AttributeBase(std::string name) : name_(std::move(name)) {}
    virtual ~AttributeBase() = default;

    const std::string& getName() const { return name_; }

    // Pure virtual function for printing
    virtual void print(StreamDevice& os) const = 0;
    
    // Add method to get type information (for schema building)
    virtual parquet::Type::type getParquetType() const { 
        // Default to string type as fallback
        return parquet::Type::BYTE_ARRAY;
    }
    
    // Add method to get converted type information
    virtual parquet::ConvertedType::type getConvertedType() const {
        // Default to UTF8 for strings
        return parquet::ConvertedType::UTF8;
    }
    
    // Add method to determine if type is required
    virtual parquet::Repetition::type getRepetitionType() const {
        return parquet::Repetition::OPTIONAL;
    }
    
    // Add a method to safely convert value to string if needed for type adaptation
    virtual std::string toString() const { 
        // Default implementation returns empty string
        return "";
    }

private:
    std::string name_;
};

// Helper function to convert various types to Parquet-compatible types
template <typename T>
auto convertToParquetType(const T& value) {
    if constexpr (std::is_same_v<T, unsigned long>) {
        if constexpr (sizeof(unsigned long) <= sizeof(uint32_t)) {
            return static_cast<uint32_t>(value);
        } else {
            return static_cast<uint64_t>(value);
        }
    } else if constexpr (std::is_same_v<T, bool>) {
        return value;
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) {
            if constexpr (sizeof(T) <= 1) return static_cast<int8_t>(value);
            else if constexpr (sizeof(T) <= 2) return static_cast<int16_t>(value);
            else if constexpr (sizeof(T) <= 4) return static_cast<int32_t>(value);
            else return static_cast<int64_t>(value);
        } else {
            if constexpr (sizeof(T) <= 1) return static_cast<uint8_t>(value);
            else if constexpr (sizeof(T) <= 2) return static_cast<uint16_t>(value);
            else if constexpr (sizeof(T) <= 4) return static_cast<uint32_t>(value);
            else return static_cast<uint64_t>(value);
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        if constexpr (sizeof(T) <= 4) return static_cast<float>(value);
        else return static_cast<double>(value);
    } else if constexpr (std::is_same_v<T, std::chrono::milliseconds> || 
                         std::is_same_v<T, std::chrono::microseconds>) {
        return value;
    } else if constexpr (std::is_same_v<T, char>) {
        return value;
    } else if constexpr (std::is_array_v<T>) {
        // try the toString function
        return  ::toString(value);
    } else if constexpr (std::is_same_v<T, const char*> || 
                         std::is_same_v<T, std::string> || 
                         std::is_same_v<T, std::string_view>) {
        // have to take a copy of the string, to ensure its lifetime is long enough
        return std::string(value);
    } else {
        // For any other type, convert to string
        return ::toString(value);
    }
}

template <typename T, typename = void>
class Attribute : public AttributeBase {
public:
    Attribute(const std::string& name, const T& value)
        : AttributeBase(name), value_(unstructured_parquet::convertToParquetType(value)), originalValue_(value) {}

    void print(StreamDevice& os) const override {
        try {
            if (value_) {
                os << *value_;
            } else {
                os << toString();
            }
        } catch (const std::exception&) {
            // If type mismatch occurs, try to convert to string
            os << toString();
        }
    }
    
    parquet::Type::type getParquetType() const override {
        if constexpr (std::is_floating_point_v<T>) {
            return parquet::Type::DOUBLE;
        } else if constexpr (std::is_integral_v<T>) {
            if constexpr (sizeof(T) <= 4) {
                return parquet::Type::INT32;
            } else {
                return parquet::Type::INT64;
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            return parquet::Type::BOOLEAN;
        } else {
            // Default to BYTE_ARRAY for strings and other types
            return parquet::Type::BYTE_ARRAY;
        }
    }
    
    parquet::ConvertedType::type getConvertedType() const override {
        if constexpr (std::is_same_v<T, int8_t>) {
            return parquet::ConvertedType::INT_8;
        } else if constexpr (std::is_same_v<T, uint8_t>) {
            return parquet::ConvertedType::UINT_8;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return parquet::ConvertedType::INT_16;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            return parquet::ConvertedType::UINT_16;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return parquet::ConvertedType::INT_32;
        } else if constexpr (std::is_same_v<T, uint32_t>) {
            return parquet::ConvertedType::UINT_32;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return parquet::ConvertedType::INT_64;
        } else if constexpr (std::is_same_v<T, uint64_t>) {
            return parquet::ConvertedType::UINT_64;
        } else if constexpr (std::is_same_v<T, std::string> || 
                            std::is_same_v<T, const char*> ||
                            std::is_same_v<T, std::string_view>) {
            return parquet::ConvertedType::UTF8;
        } else if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
            return parquet::ConvertedType::TIMESTAMP_MILLIS;
        } else if constexpr (std::is_same_v<T, std::chrono::microseconds>) {
            return parquet::ConvertedType::TIMESTAMP_MICROS;
        } else {
            return parquet::ConvertedType::NONE;
        }
    }
    
    std::string toString() const override {
        if (value_) {
            return ::toString(*value_);
        }
        return ::toString(originalValue_);
    }

private:
    std::optional<decltype(unstructured_parquet::convertToParquetType(std::declval<T>()))> value_;
    T originalValue_; // Keep the original value for toString conversion if needed
};

// Specialization for character arrays (like "static" literal strings)
template <size_t N>
class Attribute<char[N]> : public AttributeBase {
public:
    Attribute(const std::string& name, const char (&value)[N])
        : AttributeBase(name), stringValue_(value) {}

    void print(StreamDevice& os) const override {
        try {
            os << stringValue_;
        } catch (const std::exception&) {
            // If type mismatch occurs, try to convert to string
            os << toString();
        }
    }
    
    parquet::Type::type getParquetType() const override {
        return parquet::Type::BYTE_ARRAY;
    }
    
    parquet::ConvertedType::type getConvertedType() const override {
        return parquet::ConvertedType::UTF8;
    }
    
    std::string toString() const override {
        return stringValue_;
    }

private:
    std::string stringValue_; // Store as string instead of array
};

class XMLElement {
public:
    /// @brief Constructor
    explicit XMLElement(std::string name) : myName(std::move(name)), beenWritten(false) {}

    /// @brief Destructor
    virtual ~XMLElement() = default;

    /// @brief Move constructor
    XMLElement(XMLElement&& other) noexcept = default;


    /// @brief Move assignment operator
    XMLElement& operator=(XMLElement&& other) noexcept = default;

    /// @brief  Add an attribute to the XMLElement
    /// @param attr The attribute to add
    void addAttribute(std::unique_ptr<AttributeBase> attr) {
        myAttributes.push_back(std::move(attr));
    }

    // define a comparison operator (just checks the name)
    bool operator==(const XMLElement& other) const {
        return myName == other.myName;
    }

        // define a comparison operator (just checks the name)
    bool operator==(const std::string& other) const {
        return myName == other;
    }

    /// @brief a method to write the XMLElement to a stream using the << operator
    friend StreamDevice& operator<<(StreamDevice& into, const XMLElement& elem) {
        for (const auto& attr : elem.myAttributes) {
            attr->print(into);
        }
        return into;
    }

    /// @brief a method to check whether the XMLElement has been written
    bool written() const {
        return beenWritten;
    }

    /// @brief a method to set the XMLElement as written
    void setWritten() {
        beenWritten = true;
    }

    /// @brief get the attributes
    const std::vector<std::unique_ptr<AttributeBase>>& getAttributes() const {
        return myAttributes;
    }

    /// @brief get the name of the element
    const std::string& getName() const {
        return myName;
    }

    /// @brief Check if attribute exists by name
    bool hasAttribute(const std::string& name) const {
        for (const auto& attr : myAttributes) {
            if (attr->getName() == name) {
                return true;
            }
        }
        return false;
    }

    /// @brief Get attribute value as string
    std::string getAttributeAsString(const std::string& name) const {
        for (const auto& attr : myAttributes) {
            if (attr->getName() == name) {
                return attr->toString();
            }
        }
        return "";
    }

protected:
    /// @brief The name of the XMLElement
    std::string myName;

    /// @brief stores whether the XMLElement has been written
    bool beenWritten;

    /// @brief a store for the attributes
    std::vector<std::unique_ptr<AttributeBase>> myAttributes;
};

} // end namespace unstructured_parquet

/**
 * @class ParquetUnstructuredFormatter
 * @brief Output formatter for unstructured Parquet output
 *
 * ParquetUnstructuredFormatter formats output for Parquet files in an unstructured way.
 */
class ParquetUnstructuredFormatter : public OutputFormatter {
public:
    /// @brief Constructor
    ParquetUnstructuredFormatter() {};

    /// @brief Destructor
    virtual ~ParquetUnstructuredFormatter() = default;

    /** @brief Set the output type for this formatter
     * @param[in] type The type of output (e.g., "edge", "lane")
     */
    void setOutputType(const std::string& type) {
        myOutputType = type;
    }

    /** @brief Get the output type
     * @return The type of output
     */
    const std::string& getOutputType() const {
        return myOutputType;
    }

    /** @brief Check if a field exists in the schema
     * @param[in] field The field name to check
     * @return true if the field exists
     */
    bool hasField(const std::string& field) const {
        return fields.find(field) != fields.end();
    }

    /** @brief Get all fields in the schema
     * @return The set of field names
     */
    const std::set<std::string>& getAllFields() const {
        return fields;
    }

    /** @brief Reset the formatter state
     */
    void reset() {
        clearStack();
        schemaFinalized = false;
        bufferedRows.clear();
        fields.clear();
        myNodeVector.clear();
    }

    /** @brief Buffer a row for later writing
     * @param[in] row The row to buffer
     */
    void bufferRow(unstructured_parquet::XMLElement&& row) {
        bufferedRows.push_back(std::move(row));
    }

    /** @brief Get the number of buffered rows
     * @return The number of rows in the buffer
     */
    size_t getBufferedRowCount() const {
        return bufferedRows.size();
    }

    /** @brief Check if schema has been finalized
     * @return true if schema is finalized
     */
    bool isSchemaFinalized() const {
        return schemaFinalized;
    }

    /** @brief Finalize the schema
     */
    void finalizeSchema() {
        // Only finalize if we have fields to add
        if (!fields.empty() || !bufferedRows.empty()) {
            // If we have buffered rows but no fields, build the schema from the buffer
            if (fields.empty() && !bufferedRows.empty()) {
                buildNodeVectorFromBuffer();
            }
            schemaFinalized = true;
        } else {
            // Don't finalize empty schema
            schemaFinalized = false;
        }
    }

    /** @brief Get and clear buffered rows
     * @return The buffered rows
     */
    std::vector<unstructured_parquet::XMLElement> consumeBufferedRows() {
        std::vector<unstructured_parquet::XMLElement> rows = std::move(bufferedRows);
        bufferedRows.clear();
        return rows;
    }

    /** @brief Writes an XML header with optional configuration
     *
     * If something has been written (myXMLStack is not empty), nothing
     *  is written and false returned.
     *
     * @param[in] into The output stream to use
     * @param[in] rootXMLElement The root XMLElement to use
     * @param[in] attrs Additional attributes to save within the rootXMLElement
     * @todo Describe what is saved
     */
     // turn off the warning for unused parameters
    bool writeXMLHeader(StreamDevice& into, const std::string& rootXMLElement,
        const std::map<SumoXMLAttr, std::string>& attrs,
        bool includeConfig = true) override {
        UNUSED_PARAMETER(into);
        UNUSED_PARAMETER(rootXMLElement);
        UNUSED_PARAMETER(attrs);
        UNUSED_PARAMETER(includeConfig);
        return 0;
    };


    /** @brief Opens an XML tag
     *
     * An indentation, depending on the current xml-XMLElement-stack size, is written followed
     *  by the given xml XMLElement ("<" + xmlXMLElement)
     * The xml XMLElement is added to the stack, then.
     *
     * @param[in] into The output stream to use
     * @param[in] xmlXMLElement Name of XMLElement to open
     * @return The OutputDevice for further processing
     */
    void openTag(StreamDevice& into, const std::string& xmlXMLElement) override {
        UNUSED_PARAMETER(into);
        try {
#ifdef PARQUET_TESTING
            // assert that the stack does not contain the XMLElement
            assert(std::find(myXMLStack.begin(), myXMLStack.end(), xmlXMLElement) == myXMLStack.end());
#endif
            myXMLStack.push_back(unstructured_parquet::XMLElement(xmlXMLElement));
        } catch (const std::exception& e) {
            // Log the error but don't crash
            std::cerr << "Error in ParquetUnstructuredFormatter::openTag: " << e.what() << std::endl;
        }
    }

    /** @brief Opens an XML tag
     *
     * Helper method which finds the correct string before calling openTag.
     *
     * @param[in] into The output stream to use
     * @param[in] xmlXMLElement Id of the XMLElement to open
     */
    inline void openTag(StreamDevice& into, const SumoXMLTag& xmlXMLElement) override {
        openTag(into, toString(xmlXMLElement));
    };


    /** @brief Closes the most recently opened tag
     *
     * @param[in] into The output stream to use
     * @return Whether a further XMLElement existed in the stack and could be closed
     * @todo it is not verified that the topmost XMLElement was closed
     */
    inline bool closeTag(StreamDevice& into, const std::string& comment = "") override {
        UNUSED_PARAMETER(comment);
        if (myXMLStack.empty()) {
            return false;
        }
        
        try {
            // Always collect attributes from the element being closed
            if (!myXMLStack.back().getAttributes().empty()) {
                for (const auto& attr : myXMLStack.back().getAttributes()) {
                    if (fields.find(attr->getName()) == fields.end()) {
                        // This is a new field, add it to the schema
                        fields.insert(attr->getName());
                    }
                }
            }
            
            // Check if we should buffer this row instead of writing it immediately
            if (!schemaFinalized) {
                // If we're not at the top level and we have a parent that contains attributes
                // that should be propagated to children, copy them to the current element
                if (myXMLStack.size() > 1) {
                    unstructured_parquet::XMLElement& currentElement = myXMLStack.back();
                    
                    // Check if this is a lane element (typically has "_0" suffix in meandata)
                    bool isLaneElement = false;
                    const std::string& elemName = currentElement.getName();
                    if (elemName == "lane" || currentElement.getName().find("_") != std::string::npos) {
                        isLaneElement = true;
                    }
                    
                    // Identify time interval attributes from any parent in the stack
                    // These are especially important for meandata where intervals apply to all children
                    for (int i = myXMLStack.size() - 2; i >= 0; i--) {
                        unstructured_parquet::XMLElement& parentElement = myXMLStack[i];
                        
                        // These attributes should always be inherited from parent to child
                        const std::vector<std::string> inheritableAttrs = {"begin", "end", "interval", "id"};
                        
                        for (const auto& attr : parentElement.getAttributes()) {
                            const std::string& attrName = attr->getName();
                            
                            // Special handling for time attributes which should be inherited
                            // Only propagate if child doesn't already have this attribute or it's empty
                            bool shouldPropagate = std::find(inheritableAttrs.begin(), inheritableAttrs.end(), 
                                                           attrName) != inheritableAttrs.end();
                            
                            if (shouldPropagate) {
                                // Check if attribute is missing or empty in the current element
                                bool isEmpty = false;
                                if (currentElement.hasAttribute(attrName)) {
                                    // Check if it's empty
                                    std::string childValue = currentElement.getAttributeAsString(attrName);
                                    isEmpty = childValue.empty();
                                }
                                
                                // Only propagate if the attribute doesn't exist or is empty
                                if (!currentElement.hasAttribute(attrName) || isEmpty) {
                                    // For lane elements, ensure they inherit time attributes from edges
                                    std::string attrValue = attr->toString();
                                    if (!attrValue.empty()) {
                                        std::unique_ptr<unstructured_parquet::AttributeBase> attrCopy = 
                                            std::make_unique<unstructured_parquet::Attribute<std::string>>(attrName, attrValue);
                                        currentElement.addAttribute(std::move(attrCopy));
                                        
                                        // Also add to schema if needed
                                        if (fields.find(attrName) == fields.end()) {
                                            fields.insert(attrName);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Buffer the current row (last element in stack) for later writing
                bufferedRows.push_back(std::move(myXMLStack.back()));
                
                // Pop the row we just buffered
                myXMLStack.pop_back();
                return true;
            } else {
                // Schema is finalized, but we'll still just buffer for now and write at the end
                
                // Similar to above, propagate parent attributes if needed
                if (myXMLStack.size() > 1) {
                    unstructured_parquet::XMLElement& currentElement = myXMLStack.back();
                    
                    // Check if this is a lane element
                    bool isLaneElement = false;
                    const std::string& elemName = currentElement.getName();
                    if (elemName == "lane" || currentElement.getName().find("_") != std::string::npos) {
                        isLaneElement = true;
                    }
                    
                    // Look through all parents in the stack for inheritable attributes
                    for (int i = myXMLStack.size() - 2; i >= 0; i--) {
                        unstructured_parquet::XMLElement& parentElement = myXMLStack[i];
                        
                        // These attributes should always be inherited from parent to child
                        const std::vector<std::string> inheritableAttrs = {"begin", "end", "interval", "id"};
                        
                        for (const auto& attr : parentElement.getAttributes()) {
                            const std::string& attrName = attr->getName();
                            
                            // Special handling for time attributes which should be inherited
                            bool shouldPropagate = std::find(inheritableAttrs.begin(), inheritableAttrs.end(), 
                                                           attrName) != inheritableAttrs.end();
                            
                            if (shouldPropagate) {
                                // Check if attribute is missing or empty in the current element
                                bool isEmpty = false;
                                if (currentElement.hasAttribute(attrName)) {
                                    // Check if it's empty
                                    std::string childValue = currentElement.getAttributeAsString(attrName);
                                    isEmpty = childValue.empty();
                                }
                                
                                // Only propagate if the attribute doesn't exist or is empty
                                if (!currentElement.hasAttribute(attrName) || isEmpty) {
                                    std::string attrValue = attr->toString();
                                    if (!attrValue.empty()) {
                                        std::unique_ptr<unstructured_parquet::AttributeBase> attrCopy = 
                                            std::make_unique<unstructured_parquet::Attribute<std::string>>(attrName, attrValue);
                                        currentElement.addAttribute(std::move(attrCopy));
                                    }
                                }
                            }
                        }
                    }
                }
                
                if (!myXMLStack.back().written()) {
                    // Buffer all rows even after schema is finalized
                    bufferedRows.push_back(std::move(myXMLStack.back()));
                }
                // pop the last XMLElement and remove from memory
                myXMLStack.pop_back();
                return true;
            }
        } catch (const std::exception& e) {
            // Log the error but don't crash
            std::cerr << "Error in ParquetUnstructuredFormatter::closeTag: " << e.what() << std::endl;
            
            // Clear the stack to avoid further issues
            myXMLStack.clear();
            return false;
        }
    }


    /** @brief writes a preformatted tag to the device but ensures that any
     * pending tags are closed
     * @param[in] into The output stream to use
     * @param[in] val The preformatted data
     */
    void writePreformattedTag(StreamDevice& into, const std::string& val) override {
        // don't take any action
        UNUSED_PARAMETER(into);
        UNUSED_PARAMETER(val);
        return;
    };

    /** @brief writes arbitrary padding
     */
    inline void writePadding(StreamDevice& into, const std::string& val) override {
        UNUSED_PARAMETER(into);
        UNUSED_PARAMETER(val);
    };


    /** @brief writes an arbitrary attribute
     *
     * @param[in] into The output stream to use
     * @param[in] attr The attribute (name)
     * @param[in] val The attribute value
     */
    template <class T>
    void writeAttr(StreamDevice& into, const std::string& attr, const T& val) {
        UNUSED_PARAMETER(into);
        try {
            // Safety check to prevent crash when stack is empty
            if (myXMLStack.empty()) {
                std::cerr << "Error: Cannot write attribute '" << attr << "' - XML stack is empty" << std::endl;
                return;
            }
            
            std::unique_ptr<unstructured_parquet::AttributeBase> typed_attr = 
                std::make_unique<unstructured_parquet::Attribute<T>>(attr, val);
                
            this->myXMLStack.back().addAttribute(std::move(typed_attr));
            
            // Only add to node vector if schema is finalized
            if (schemaFinalized && !sharedNodeVector && this->fields.find(attr) == this->fields.end()) {
                // Add the field to the schema - with try/catch to handle potential errors
                try {
                    unstructured_parquet::AppendField(myNodeVector, val, attr);
                    this->fields.insert(attr);
                } catch (const std::exception& e) {
                    std::cerr << "Error adding field to schema: " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in ParquetUnstructuredFormatter::writeAttr: " << e.what() << std::endl;
        }
    }

    /** @brief Build node vector from buffered rows
     * This analyzes all buffered rows to create a complete schema
     */
    void buildNodeVectorFromBuffer() {
        // Clear existing node vector
        myNodeVector.clear();
        
        // Create a map to track attribute types by field name
        std::map<std::string, std::pair<parquet::Type::type, parquet::ConvertedType::type>> fieldTypes;
        
        // First pass: collect all field names and their types from the buffer
        for (const auto& row : bufferedRows) {
            for (const auto& attr : row.getAttributes()) {
                const std::string& name = attr->getName();
                if (fields.find(name) == fields.end()) {
                    fields.insert(name);
                    
                    // Try to get type information from the attribute
                    parquet::Type::type pType = attr->getParquetType();
                    parquet::ConvertedType::type cType = attr->getConvertedType();
                    
                    // Store the type information
                    fieldTypes[name] = std::make_pair(pType, cType);
                }
            }
        }
        
        // Second pass: create schema nodes with the collected type information
        for (const auto& field : fields) {
            auto it = fieldTypes.find(field);
            if (it != fieldTypes.end()) {
                // Use the detected type information
                parquet::Type::type pType = it->second.first;
                parquet::ConvertedType::type cType = it->second.second;
                
                // Create the appropriate node based on the type
                if (pType == parquet::Type::DOUBLE) {
                    myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                        field, parquet::Repetition::OPTIONAL, pType, cType));
                } else if (pType == parquet::Type::INT32 || pType == parquet::Type::INT64) {
                    myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                        field, parquet::Repetition::OPTIONAL, pType, cType));
                } else if (pType == parquet::Type::BOOLEAN) {
                    myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                        field, parquet::Repetition::OPTIONAL, pType, cType));
                } else {
                    // Default to BYTE_ARRAY for everything else
                    myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                        field, parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
                        parquet::ConvertedType::UTF8));
                }
            } else {
                // Fallback to string type if no type info is available
                myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                    field, parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
                    parquet::ConvertedType::UTF8));
            }
        }
        
        // If we have rows but no fields were found (unlikely but possible),
        // create a dummy field to ensure we have a valid schema
        if (!bufferedRows.empty() && myNodeVector.empty()) {
            // Add a dummy field to ensure we have a valid schema
            myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                "dummy", parquet::Repetition::OPTIONAL, parquet::Type::BYTE_ARRAY,
                parquet::ConvertedType::UTF8));
            fields.insert("dummy");
        }
        
        // Mark schema as finalized
        schemaFinalized = true;
    }

    /** @brief returns the node vector
     * @return const parquet::schema::NodeVector&
    */
    inline const parquet::schema::NodeVector& getNodeVector() {
        if (!schemaFinalized && !bufferedRows.empty()) {
            buildNodeVectorFromBuffer();
        }
        
        // If we have a schema but no fields/nodes yet, try to build them
        if (myNodeVector.empty() && !bufferedRows.empty()) {
            buildNodeVectorFromBuffer();
        }
        
        sharedNodeVector = true;
        return myNodeVector;
    }

    bool wroteHeader() const override {
        return !myXMLStack.empty();
    }

    /**
     * @brief Get the Stack object
     *
     * @return std::vector<_Tag *>&
     */
    inline std::vector<unstructured_parquet::XMLElement>& getStack() {
        return myXMLStack;
    }

    /**
     * @brief Write the header. (This has no effect for the ParquetUnstructuredFormatter)
     *
     * @param Return success
     */
    inline bool writeHeader([[maybe_unused]] StreamDevice& into, [[maybe_unused]] const SumoXMLTag& rootElement) override { return true; };

    
    template <typename T>
    void writeRaw(StreamDevice& into, T& val) {
        UNUSED_PARAMETER(into);
        UNUSED_PARAMETER(val);
        throw std::runtime_error("writeRaw not implemented for ParquetUnstructuredFormatter");
    }

    int getDepth() const {
        return static_cast<int>(myXMLStack.size());
    }

    void clearStack() {
        myXMLStack.clear();
        myNodeVector.clear();
        fields.clear();
    }

    /** @brief Add a field to the schema with specific type information
     * @param[in] fieldName Name of the field to add
     * @param[in] pType Parquet physical type
     * @param[in] cType Parquet converted type
     */
    void addFieldToSchema(const std::string& fieldName, 
                          parquet::Type::type pType = parquet::Type::BYTE_ARRAY,
                          parquet::ConvertedType::type cType = parquet::ConvertedType::UTF8) {
        // Only add if it doesn't already exist
        if (fields.find(fieldName) == fields.end()) {
            fields.insert(fieldName);
            
            // Only add to node vector if schema is already finalized
            if (schemaFinalized && !sharedNodeVector) {
                myNodeVector.push_back(parquet::schema::PrimitiveNode::Make(
                    fieldName, parquet::Repetition::OPTIONAL, pType, cType));
            }
        }
    }

private:
    /// @brief The stack of begun xml XMLElements.
    /// We don't need to store the full XMLElement, just the value
    std::vector<unstructured_parquet::XMLElement> myXMLStack;

    /// @brief The parquet node vector
    parquet::schema::NodeVector myNodeVector;

    /// @brief flag to determin if we have shared NodeVector
    bool sharedNodeVector{false};

    /// @brief flag to determine if schema is finalized
    bool schemaFinalized{false};

    /// @brief Buffer for rows before schema is finalized
    std::vector<unstructured_parquet::XMLElement> bufferedRows;

    // @brief the set of unique fields
    std::set<std::string> fields;

    /// @brief The type of output (e.g., "edge", "lane")
    std::string myOutputType;
};

#endif // HAVE_PARQUET